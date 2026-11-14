# Intégration des références UniXNU-kebab au port OnePlus 8 IN2013

Date de l'audit : 7 novembre 2026

## Décision technique

Le dépôt UniXNU-kebab de GFander est retenu comme référence principale pour les mécanismes XNU/SM8250 déjà éprouvés. Le dépôt de stevedang67889 est une référence complémentaire pour l'UEFI, DarwinBoot, la collecte de logs et les essais de démarrage.

**Aucun code matériel spécifique à kebab ne doit être copié tel quel dans la cible instantnoodle.** Les deux téléphones partagent le Snapdragon 865/SM8250, mais les périphériques et certaines séquences d'initialisation diffèrent.

## Références examinées

- GFander : https://github.com/GFander/UniXNU-kebab
- stevedang67889 : https://github.com/stevedang67889/UniXNU-kebab
- État UniXNU publié : https://unixnu.com/devices/kebab
- Port cible IN2013 : https://github.com/samote152-dev/OnePlus8-IN2013-XNU-Port

Les dépôts kebab sont des projets de portage indépendants. Leurs scripts/configurations s'appuient sur le système de build UniXNU et ne sont pas interchangeables automatiquement avec le workflow historique `darwin-xnu-build` déjà présent ici.

## Éléments réutilisables après adaptation

1. **Méthode de build XNU minimal** : defconfig minimal, archives de support et stubs faibles pour isoler le démarrage du noyau des pilotes non essentiels.
2. **Ordre de bring-up** : démarrer avec un seul CPU, console framebuffer/UART, GIC/timer, puis seulement ensuite UFS/root et les autres périphériques.
3. **Méthode ADT/DTB** : générer l'ADT depuis le DTB vivant de l'appareil et contrôler les plages mémoire, GIC, timer, UFS et framebuffer.
4. **DarwinBoot / hand-off** : reprendre les contrôles de transfert mémoire, ExitBootServices et les marqueurs de diagnostic, en les raccordant au DTB et au boot chain IN2013.
5. **UFS** : étudier l'architecture du pilote et les hooks de publication de volume. Ne reprendre les registres/quirks qu'après comparaison avec le DTB et les journaux IN2013.

## Différences matérielles à respecter

| Élément | OnePlus 8 IN2013 (cible) | OnePlus 8T kebab (référence) |
|---|---|---|
| SoC | SM8250 / Snapdragon 865 | SM8250 / Snapdragon 865 |
| Codename | instantnoodle | kebab |
| Écran | Samsung AMB655UV01, 1080×2400, 90 Hz | Samsung AMB655X, 1080×2400, 120 Hz |
| Tactile | Samsung S6SY771, I²C, adresse 0x48 | Synaptics S3908, I²C, adresse 0x4B |
| Stockage | UFS 3.0 | UFS 3.1 |
| PMIC / GPIO / clocks | valeurs IN2013 issues de son DTB vivant | ne pas supposer identiques |

La documentation du noyau Linux sur kebab indique notamment que le OnePlus 8 ne possède pas le PM8008 présent sur le 8T. Il faut donc éviter de recopier la configuration PMIC du 8T.

## Base IN2013 à conserver

Le profil matériel existant dans ce dépôt reste l'autorité pour la cible :
- `build/target-profile.toml`
- `device/instantnoodle.json`
- `device/instantnoodle-live.dtb`
- `device/instantnoodle-root-audit.txt`
- `board/instantnoodle/`

Ne pas remplacer ces données par `board.toml`, `kebab_defconfig`, le DTB kebab ou les valeurs de panneau/tactile de l'8T.

## Plan d'intégration sûr

- [x] Examiner les deux dépôts kebab et l'état public du port.
- [x] Identifier les composants communs SM8250 et les différences matérielles.
- [x] Isoler les approches transférables de celles qui sont spécifiques au 8T.
- [ ] Comparer les sources exactes de DarwinBoot/ADT et le pipeline de build avec le workflow IN2013.
- [ ] Ajouter une cible de build minimal IN2013 inspirée de la configuration kebab, sans remplacer le workflow actuel.
- [ ] Construire et inspecter le Mach-O XNU sur macOS CI.
- [ ] Vérifier que le kernel, l'ADT et les arguments de boot correspondent au DTB IN2013.
- [ ] Seulement après ces validations, préparer un test temporaire non destructif. Aucun flash de partition n'est autorisé dans cette étape.

## Limites de validation connues

Les résultats publics UniXNU-kebab ne signifient pas qu'un iOS complet démarre. La page d'état UniXNU décrit un noyau qui atteint un root HFS et exécute un programme de test statique en EL0 ; le userland Apple complet, les CPU secondaires et l'intégration USB restent en cours. L'autre dépôt indique encore des blocages autour de la console/GIC et de la découverte UFS.

Le dépôt IN2013 indique lui-même que le noyau XNU complet n'a pas encore été compilé. Cette intégration documentaire ne change pas cet état et ne constitue pas une validation de compilation ou de démarrage.

