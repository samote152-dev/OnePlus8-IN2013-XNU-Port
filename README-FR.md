# Port XNU / iOS vers OnePlus 8 IN2013

Ce dépôt poursuit le travail Apollo existant. Il ne remplace pas la méthode par un nouveau projet : il ajoute une cible **INSTANTNOODLE** au XNU 8796.141.3 et conserve les adaptations SM8250 déjà présentes.

## Ce qui a réellement été préparé

- Base XNU : `xnu-8796.141.3`, la version qui correspond le mieux à la série de patches fournie. Sur cette base, **166 patches sur 168** s'appliquent directement. Les deux patches HFS restants dépendent des sources HFS récupérées par les scripts de staging du build.
- Nouveau patch `0169-instantnoodle-board-target.patch` : cible machine `INSTANTNOODLE`, compatibilité Device Tree `oneplus,instantnoodle`, CPU `qcom,kryo`, timer virtuel GIC INTID 27, et désactivation des périphériques spécifiques au Xiaomi Mi 10T tant que leurs équivalents IN2013 ne sont pas validés.
- ADT IN2013 : `tools/mkadt-in2013.py` utilise les plages GIC, UFS, mémoire et framebuffer extraites du DTB vivant fourni.
- Shim AArch64 : `shim/prebuilt-Image` a été compilé avec Clang/LLD dans l'environnement Linux. Il fait 167 936 octets et ne contient aucune relocation ABS64 dans les objets C.
- Tactile : le protocole S6SY771 et le pont HID portable ont passé **4/4 tests hôte**. Le pilote n'est pas encore activé dans XNU : les clocks, le pinmux GENI, la ligne IRQ et la séquence reset doivent d'abord être intégrés et vérifiés.
- Repack Android boot v2 : `tools/repack-boot-v2.py` remplace kernel/ramdisk en préservant le DTB de l'image originale. Il enlève la signature/footer AVB et produit une image non signée pour test temporaire uniquement.

## Ce qui n'est PAS encore fait

- Le noyau XNU complet n'a pas encore été compilé : cette étape utilise le toolchain Apple/Xcode (`iig`, SDK Kernel) du workflow macOS GitHub Actions.
- Aucun noyau n'a encore démarré sur le OnePlus 8.
- Aucun root filesystem iOS n'est préparé pour le téléphone. Le kernel peut être construit et testé jusqu'au point d'initialisation BSD, mais il ne pourra pas atteindre launchd/SpringBoard tant que le volume HFS iOS n'est pas préparé. Le root iOS Apollo existant remplace `userdata` Android et efface donc les données Android ; aucune opération de ce type n'est incluse ici.
- Le backend d'affichage DSI Samsung n'est pas écrit. Le framebuffer de splash est identifié, mais son format exact et son stride doivent être confirmés à partir de l'ABL avant de lui écrire.
- Le pilote tactile S6SY771 n'est pas encore raccordé au contrôleur GENI dans le noyau.
- Les firmwares GPU, Wi-Fi, Bluetooth, audio, PMIC et rétroéclairage ne sont pas intégrés.
- Le nœud Apple `/product` du Device Tree iPhone X n'est pas encore inclus : il faudra l'extraire de l'IPSW retenu pour le userland.

## Version iOS : point à ne pas masquer

Le profil du kit Apollo indique iOS **16.7.16 / 20H392**, alors que la version demandée précédemment était **16.5**. La base XNU existante est `8796.141.3`. Les archives disponibles ne permettent pas d'affirmer que ce XNU est exactement celui d'iOS 16.5 ; l'essai avec `8796.101.5` fait échouer 14 patches. Pour ne pas changer de méthode ni casser la base existante, le projet garde `8796.141.3` et marque la compatibilité du userland 16.5 comme non vérifiée.

## Validation locale déjà exécutée

```bash
python3 tools/validate-repo.py
python3 tools/mkadt-in2013.py /tmp/instantnoodle.adt --highmem --native-display
```

La validation du dépôt passe avec 169 patches numérotés de `0001` à `0169`. Le patch 0169 s'applique proprement après la base Apollo.

## Construire XNU

1. Mettre ce dossier dans un dépôt GitHub.
2. Ouvrir **Actions → xnu → Run workflow**.
3. Garder `machine_config = INSTANTNOODLE` et `kernel_config = DEVELOPMENT`.
4. Le workflow construit XNU sur macOS, génère l'ADT IN2013, prépare `xnu.payload` et compile le shim.
5. Récupérer l'artifact `xnu-kernel`.

Le workflow n'effectue aucun flash et ne modifie pas le téléphone.

## Préparer une image temporaire (après compilation uniquement)

Avec le `boot.img` original de l'IN2013, le `shim-Image` et le `xnu.payload` de l'artifact :

```bash
python3 tools/repack-boot-v2.py boot.img shim-Image xnu.payload boot-in2013-test.img
```

L'image obtenue est non signée (footer AVB retiré). Elle est destinée à `fastboot boot` sur un bootloader déverrouillé, **pas à `fastboot flash`**. Ne pas lancer ce test tant que le noyau XNU n'a pas été compilé et contrôlé.
