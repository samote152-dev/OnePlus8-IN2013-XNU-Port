# Audit des artefacts XNU — run 42460601750

Date : 8 novembre 2026
Branche : `integration/unixnu-kebab-reference`
Commit de build : `370de37dcce735365bf125c5e000c572fed11abe`

## Conclusion

La compilation du noyau a bien produit un exécutable Mach-O 64 bits ARM64 non vide, et le workflow a terminé avec succès. Le binaire est identifié dans ses chaînes comme `Darwin Kernel Version 22.6.0`, `xnu-8796.141.3`, et contient l'identité de plateforme `oneplus,instantnoodle`, ainsi que les marqueurs du pilote UFS (`SWEET UFS:`, `ufsroot0`).

Cela prouve une compilation et un packaging cohérents pour la cible déclarée. Cela ne prouve pas encore que le noyau démarre sur le téléphone.

## Jobs et étapes GitHub Actions

- `validate` : success.
- `build` : success.
- Build XNU : success.
- Vérification du patch series et de l'identité INSTANTNOODLE : success.
- Génération ADT et payload : success.
- Build du shim AArch64 : success.
- Upload des artefacts : success.

Le job ath11k séparé est marqué skipped car la cible est `INSTANTNOODLE`; l'en-tête WLFW généré a été intégré. Ne pas interpréter ce skip comme un échec de build.

## Vérification locale des fichiers téléchargés

Artefact `xnu-kernel` (ID 13609063373) :
- `kernel.development.instantnoodle` : 17 855 504 octets.
- `in2013/xnu.payload` : 17 211 296 octets.
- `in2013/instantnoodle.adt` : 10 132 octets.
- `in2013/shim-Image` : 167 936 octets.

Mach-O :
- Magic `0xfeedfacf` (Mach-O 64-bit little-endian).
- CPU type `0x0100000c` (ARM64).
- File type `2` (executable).
- 26 load commands, tailles cohérentes avec l'en-tête.
- Chaînes : `Darwin Kernel Version 22.6.0`, `xnu-8796.141.3`, `oneplus,instantnoodle`, `SWEET UFS:`, `ufsroot0`.
- SHA-256 : `d2dead62e7b61cda29b23401d85713810cf89311c69168385373abf21f2a3a06` (correspond au fichier SHA256SUMS de l'artefact).

Payload :
- Magic `SWEETXNU`, version 1.
- 10 segments ; toutes les plages de données vérifiées restent dans le fichier.
- Entrée Mach-O `0xfffffe00074b0488`, située dans le segment noyau attendu.
- ADT incorporé : offset `0x01067800`, taille `0x2794` (10 132 octets).
- RAM déclarée : base `0x100000000`, taille `0xfff00000`.
- Cmdline : `debug=0xc serial=3 cpus=1 keepsyms=1 sweetfb_native=1`.

ADT :
- Parse complète : 10 132 octets consommés, aucun octet résiduel.
- 8 nœuds CPU présents.
- GIC compatible `arm,gic-v3`, virtual timer INTID 27.
- Nœud `sweet-ufs` présent avec base MMIO `0x01d84000`, taille `0x3000`.
- Framebuffer à `0x9c000000`, 1080 × 2400, stride 4320.
- `chosen/dram-base` = `0x100000000`, `dram-size` = `0xfff00000`.

Shim :
- `file` identifie `shim-Image` comme une image boot ARM64 little-endian, 16K pages.
- Sa présence et sa génération sont confirmées ; aucun test de lancement sur appareil n'a été fait.

## Réserve importante : identité /product

Le log du workflow signale :
`research/ipsw-x/d22ap.adt missing, no /product node`.

Le script `mkadt-in2013.py` copie les propriétés /product de l'ADT D22AP lorsqu'il est disponible. Comme ce fichier est absent, l'ADT final ne contient pas de nœud `product`. La génération du payload continue malgré cet avertissement.

C'est un point à corriger avant de considérer le hand-off au userland iOS comme prêt. Il ne remet pas en cause la compilation du Mach-O XNU, mais peut affecter la détection d'identité et le démarrage des composants du userland.

## Prochaines vérifications avant tout essai matériel

1. Obtenir et intégrer uniquement les propriétés /product nécessaires depuis une source D22AP légalement fournie par l'utilisateur ou une récupération vérifiable, sans remplacer les nœuds matériels IN2013.
2. Vérifier le contenu exact du payload et les adresses physiques / réservations par rapport au DTB vivant IN2013, notamment les carve-outs Qualcomm.
3. Revoir le hand-off UEFI/boot shim, l'EL et l'état GIC laissé par l'ABL.
4. Préparer un protocole de test temporaire et récupérable (sans flash de partition) et une méthode de collecte de logs.

Aucun démarrage sur téléphone, aucun flash et aucun fonctionnement iOS ne sont revendiqués par cet audit.
