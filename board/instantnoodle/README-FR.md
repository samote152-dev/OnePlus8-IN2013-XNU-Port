# Matériel IN2013 et pilotes réutilisables

Les valeurs ci-dessous proviennent du DTB vivant `device/instantnoodle-live.dtb`, du profil kit et des références Qualcomm/Linux fournies.

| Élément | Valeur | Statut |
|---|---|---|
| SoC | Qualcomm SM8250 / Snapdragon 865 | profil IN2013 |
| GICD / GICR | 0x17a00000 / 0x17a60000 | DTB vivant |
| Timer virtuel | PPI 11 → INTID 27 | DTB vivant |
| UFS | 0x01d84000, taille 0x3000 | DTB vivant + audit |
| Framebuffer splash | 0x9c000000, région 0x02300000 | DTB vivant |
| Écran | Samsung AMB655UV01, 1080×2400 | profil kit |
| GENI SE13 | moteur 0x00a94000, wrapper 0x00ac0000 | DTB vivant |
| Tactile | S6SY771, I²C 0x48, IRQ GPIO39, reset GPIO38 | DTB vivant |

La région framebuffer et le stride de 4320 octets sont utilisés par le shim Apollo existant. Le format pixel A8R8G8B8 reste une hypothèse à confirmer par l'ABL avant de faire écrire le noyau dans cette mémoire.

## Pourquoi le tactile n'est pas activé

Le code `geni/NativeGeniI2c.*` est la référence native I²C GENI fournie. Le code `touch/S6SY771` contient le parseur d'événements et le pont HID portable, testé 4/4. Il manque encore la configuration sûre des clocks SE13, du pinmux GPIO36/37, du reset GPIO38 et de l'IRQ GPIO39 dans XNU. Lancer des transactions I²C sans ces ressources serait prématuré ; le premier profil de démarrage laisse donc les périphériques Mi 10T désactivés.
