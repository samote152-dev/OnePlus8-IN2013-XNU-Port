# IN2013 — audit d'intégration Apollo/XNU et plan du bridge v5

## Sources réellement examinées

Snapshot Apollo local : branche `instantnoodle-port`, commit `c0b4400685fb8e23dcb28a1ec2dba4bc7343fe3a`, état Git propre au moment de la collecte. Le snapshot contient 168 patchs, les scripts et le shim, mais pas le checkout complet des sources XNU ni tous les headers nécessaires à une compilation noyau.

Patchs pertinents :
- `0058-sweet-hid-event-services.patch` ajoute les services HID SWEET et leur enregistrement dans `KernelConfigTables.cpp`.
- `0060-sweet-keys-touch-drivers.patch` ajoute `bsd/dev/sweet_touch.c` et l'appelle depuis l'initialisation UFS. Ce pilote est spécifique à SWEET/Mi 10T : ses adresses, contrôleurs et séquences ne doivent pas être copiés sur IN2013.
- `0063-sweet-goodix-geni-packing.patch` adapte le packing FIFO GENI pour un contrôleur Goodix à l'adresse 0x5d. Sa mécanique FIFO est une référence, mais le protocole Goodix, son registre d'événement et ses séquences d'initialisation ne sont pas réutilisables pour le Samsung S6SY771.
- `0082-apollo-novatek-touch.patch` et `0088-apollo-touch-live-dt-spi.patch` concernent le Novatek NT36675 sur SPI, pas le S6SY771 sur I²C.

## Architecture proposée

Le bridge v5 introduit une frontière testable entre :
1. le cœur S6SY771 portable déjà validé ;
2. les opérations de carte à fournir par un futur backend XNU (initialisation/arrêt du bus, état IRQ, lecture I²C) ;
3. le callback HID déjà abstrait.

Le code v5 n'effectue aucun MMIO, aucun accès GPIO, aucune configuration d'horloge ou PMIC et ne déclare pas être un pilote IOKit compilable. Il teste le cycle de vie et l'aiguillage IRQ avec un backend simulé.

## Informations carte à confirmer avant tout backend matériel

Références du snapshot OnePlus 8 récupéré : SE13 GENI `0x00A94000`, wrapper QUPv3 `0x00AC0000`, bus I²C GPIO36/37, adresse S6SY771 `0x48`, IRQ GPIO39, reset GPIO38. Les adresses et les états pinctrl doivent être recoupés avec le DTB vivant et les fichiers complets du même commit avant usage.

Le précédent audit a relevé que les labels pinctrl mentionnés par le nœud tactile ne sont pas tous retrouvés dans les extraits collectés. Les rails PMIC, le séquencement d'alimentation, la fréquence effective de SE13 et la disponibilité du firmware GENI restent à confirmer. Ne pas coder des valeurs supposées.

## Étapes bloquantes suivantes

1. Obtenir le checkout XNU exact correspondant à XNU-8796/ios-apollo et vérifier les API IOKit, les headers HID et le système d'enregistrement de drivers.
2. Revoir le code GENI du patch 0063 ligne par ligne et extraire uniquement la mécanique FIFO générique, sans les commandes Goodix ni les valeurs spécifiques SWEET.
3. Implémenter un backend GENI SM8250 en lecture seule au départ, avec timeouts et diagnostics ; aucun reset/écriture PMIC automatique.
4. Ajouter un service IOKit/HID en utilisant les classes et interfaces exactes présentes dans le checkout complet.
5. Compiler le noyau complet sur la chaîne supportée par Apollo et vérifier l'ordre d'application des patchs.
6. Faire une revue de sécurité et établir une méthode de démarrage de récupération avant tout essai sur le téléphone.

## Limites du test

Les tests de ce paquet sont des tests logiciels simulés. Ils ne valident ni les registres GENI du SM8250, ni le contrôleur tactile physique, ni la compilation XNU/IOKit, ni un démarrage iOS.
