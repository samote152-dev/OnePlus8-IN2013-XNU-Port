# État du port IN2013 — 7 novembre 2026

## Progression vers le premier test sur téléphone

**Préparation de la base : avancée. Premier démarrage XNU : pas encore atteint.** Aucun pourcentage global n'est attribué tant que le build XNU n'est pas exécuté.

### Validé
- XNU 8796.141.3 choisi car il correspond à la série Apollo fournie.
- 166/168 patches appliqués sur cette archive source ; 0031 et 0041 attendent les sources HFS staged.
- 0169 crée la cible OnePlus INSTANTNOODLE et passe `git apply --check`.
- DTB vivant analysé : mémoire, GIC, timer, UFS, framebuffer réservé, GENI SE13 et S6SY771.
- ADT IN2013 généré et inspecté : mémoire gérée 4 GiB à partir de 0x100000000, GIC timer INTID 27, UFS 0x1d84000, framebuffer 0x9c000000.
- Shim AArch64 compilé avec Clang/LLD ; 0 relocation ABS64 dans les objets C.
- Tests hôte S6SY771 : 4/4.
- Repack boot v2 vérifié sur l'image fournie : le DTB est conservé octet pour octet.
- `validate-repo.py` : 169 patches et outils validés.

### À faire avant le premier `fastboot boot`
1. Exécuter le workflow XNU sur macOS/Xcode et corriger les éventuelles erreurs de compilation.
2. Contrôler le Mach-O kernel et générer `xnu.payload` avec l'ADT IN2013.
3. Confirmer l'ABL framebuffer (pixel format/stride) avant toute écriture à l'écran.
4. Repackager le boot v2 à partir du boot.img original, sans AVB footer, pour un test temporaire.
5. Tester uniquement avec `fastboot boot`, jamais en flashant une partition.
6. Préparer ensuite un root filesystem iOS à partir de l'IPSW exact. Le root iOS Apollo remplace le contenu `userdata` Android ; cette étape est destructive et est volontairement exclue du présent travail.

### À faire après le premier démarrage
- GENI clocks/pinctrl/reset/IRQ et intégration S6SY771.
- Backend panel Samsung AMB655UV01 / MIPI-DSI.
- PMIC, charge, Wi-Fi, Bluetooth, audio, GPU/GMU et rétroéclairage.
- Extraction du Device Tree `/product` depuis l'IPSW exact retenu.
- Vérification de compatibilité entre XNU 8796.141.3 et le userland demandé (16.5 vs profil Apollo 16.7.16).
