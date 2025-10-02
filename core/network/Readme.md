# Module `network` — Intégration & Exécution

## Dépendances
- **libIEC61850** (MzAutomation) installée (headers + lib)
- **libpcap** (Linux) / **NPcap** (Windows) si vous utilisez GOOSE/SV ou la relecture PCAP
- C++17, CMake ≥ 3.18

## Droits & permissions
- **Linux** : GOOSE/SV utilisent des sockets raw. Exécutez en `root` **ou** donnez le droit:
  ```bash
  sudo setcap cap_net_raw+ep <chemin-vers-binaire>
