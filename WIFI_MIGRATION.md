# Clawdmeter — Migration WiFi direct (sans démon BLE)

## Contexte

Actuellement, Clawdmeter fonctionne ainsi :
```
ESP32 ←BLE GATT→ Démon bash (PC) → api.anthropic.com
```

Le démon tourne sur le PC, lit un token OAuth, fait un appel API minimal (1 token Haiku), et pousse le résultat JSON en BLE GATT vers l'ESP32.

**Objectif :** supprimer le démon BLE et faire poll l'ESP32 via WiFi un serveur local qui expose l'usage.

```
ESP32 (WiFi/HTTP) → Serveur local → api.anthropic.com (HTTPS)
```

Le serveur local gère le TLS avec Anthropic — l'ESP32 fait du HTTP simple sur le réseau local, sans avoir à négocier TLS.

> **Pourquoi pas appel direct ESP32 → Anthropic ?**
> `api.anthropic.com` impose TLS 1.3 avec le groupe post-quantique `X25519MLKEM768` (vérifié openssl). mbedTLS sur ESP32 ne supporte pas ce groupe, ce qui risque de bloquer le handshake même avec `setInsecure()`.

---

## Ce que expose le serveur local

L'ESP32 poll un endpoint HTTP configuré, par exemple :
```
http://192.168.31.100:1234/api/usage
```

Réponse JSON attendue :
```json
{ "usagePercent5h": 11, "resetIn5h": 17273, "usagePercent7d": 1, "resetIn7d": 434273, "status": "allowed", "ok": true, "fetchedAt": 1779999726801 }
```
- `usagePercent5h` = session % (5h utilization × 100)
- `resetIn5h` = secondes avant reset 5h
- `usagePercent7d` = weekly % (7d utilization × 100)
- `resetIn7d` = secondes avant reset 7j
- `status` = status string (`"allowed"` ou `"rate_limited"`)
- `ok` = succès de l'appel côté serveur
- `fetchedAt` = timestamp ms (ignoré par le firmware)

Le serveur local est responsable de :
- Stocker le token OAuth (`claude setup-token`, validité 1 an)
- Appeler `api.anthropic.com/v1/messages` (1 token Haiku)
- Lire les headers `anthropic-ratelimit-unified-*`
- Exposer le résultat en JSON via HTTP

---

## Architecture des fichiers (état actuel)

```
firmware/src/
  main.cpp              — setup(), loop(), boutons, rotation
  display_cfg.h         — defines pins
  ui.{h,cpp}            — 3 écrans : splash, usage, bluetooth
  splash.{h,cpp}        — moteur animation pixel-art 20×20
  ble.{h,cpp}           — NimBLE : custom GATT data service + HID keyboard
  data.h                — struct UsageData
  imu.{h,cpp}           — rotation accéléromètre
  power.{h,cpp}         — AXP2101 (batterie, VBUS, bouton PWR)
  touch.{h,cpp}         — détecteur tap
  icons.h / logo.h      — assets RGB565
  font_*.c              — polices LVGL 9
  splash_animations.h   — généré, ne pas éditer

daemon/
  claude-usage-daemon.sh   — À SUPPRIMER
  claude-usage-daemon.service — À SUPPRIMER
install.sh                 — À SUPPRIMER (ou adapter)
```

---

## TODO

### 1. Supprimer le démon
- [ ] Supprimer `daemon/claude-usage-daemon.sh`
- [ ] Supprimer `daemon/claude-usage-daemon.service`
- [ ] Adapter ou supprimer `install.sh`

### 2. Créer `config.{h,cpp}` — provisioning WiFi + endpoint
- [ ] Au premier démarrage (NVS vide), passer en mode AP (`Clawdmeter-Setup`)
- [ ] Servir une page web minimaliste : champs SSID, password, URL de l'endpoint (ex: `http://192.168.31.100:1234/api/usage`)
- [ ] Sauvegarder les valeurs en NVS via `Preferences`
- [ ] Redémarrer après sauvegarde

### 3. Créer `wifi_manager.{h,cpp}`
- [ ] Connexion WiFi au démarrage avec les credentials NVS
- [ ] Reconnexion automatique en cas de perte
- [ ] Exposer `wifi_is_connected()` et `wifi_ip_str()`

### 4. Créer `api.{h,cpp}`
- [ ] Appel HTTP `GET` vers l'endpoint configuré en NVS (ex: `http://192.168.31.100:1234/api/usage`)
- [ ] Utiliser `HTTPClient` (pas `WiFiClientSecure` — HTTP simple, pas de TLS)
- [ ] Parser le JSON de réponse (`ArduinoJson`)
- [ ] Remplir et retourner un `UsageData`
- [ ] Gérer les erreurs réseau et HTTP (retry simple, timeout)

### 5. Modifier `ble.{h,cpp}`
- [ ] Supprimer le custom GATT data service (`4c41555a-...` RX/TX/REQ)
- [ ] Supprimer `onWrite`, parsing JSON entrant, `has_received_data`, caractéristique REQ
- [ ] Garder uniquement le **HID keyboard** (boutons Space / Shift+Tab)

### 6. Modifier `main.cpp`
- [ ] Remplacer l'init BLE data service par `wifi_manager_init()` + `api_init()`
- [ ] Remplacer la logique de poll BLE (tick 5s / 60s) par un timer `api_fetch()` toutes les 60s
- [ ] Garder le BLE HID init inchangé

### 7. Modifier `ui.{h,cpp}`
- [ ] Renommer/refaire l'écran "Bluetooth" en écran "WiFi / Status"
- [ ] Afficher : IP locale, force signal WiFi (RSSI), statut dernière requête API, `representative_claim` actif

### 8. Modifier `data.h`
- [ ] Renommer `session_reset_mins` → `session_reset_secs` et `weekly_reset_mins` → `weekly_reset_secs` (le JSON renvoie `sr`/`wr` en secondes — aligner les champs)
- [ ] Ajouter champ `representative_claim` (`enum { CLAIM_5H, CLAIM_7D }`) — calculé par l'ESP32 : `session_pct >= weekly_pct ? CLAIM_5H : CLAIM_7D`
- [ ] Optionnel : ajouter tokens restants si utile pour l'affichage

### 9. Modifier `platformio.ini`
- [ ] Aucune dépendance à ajouter : `ArduinoJson ^7.0.0` déjà présent, `WiFiClient`/`HTTPClient`/`DNSServer` inclus dans le core ESP32

### 10. Tests
- [ ] Vérifier la connexion WiFi au boot et la reconnexion
- [ ] Vérifier l'appel API et la réception des headers
- [ ] Vérifier que les boutons HID fonctionnent toujours indépendamment
- [ ] Vérifier le portail captif de config sur un téléphone
- [ ] Vérifier le comportement si le token est expiré (afficher erreur sur l'écran status)

---

## Notes importantes

- Le BLE HID (boutons Space/Shift+Tab) **coexiste** avec le WiFi sur l'ESP32-S3, pas de conflit.
- NVS survive aux OTA et aux flash du firmware (sauf `pio run -t erase`).
- L'endpoint est stocké en NVS et configurable via le portail captif — pas besoin de recompiler pour changer d'adresse IP.
- Le serveur local doit être joignable en permanence (NAS, Pi, machine toujours allumée). Si l'endpoint est down, l'ESP32 affiche la dernière valeur connue + une indication d'erreur.

### Pourquoi un serveur local et pas appel direct

`api.anthropic.com` est passé sur Google Cloud et impose TLS 1.3 avec le groupe post-quantique `X25519MLKEM768`. mbedTLS sur ESP32 ne supporte pas ce groupe. Le serveur local absorbe cette complexité TLS — l'ESP32 fait du HTTP plain sur le LAN.
