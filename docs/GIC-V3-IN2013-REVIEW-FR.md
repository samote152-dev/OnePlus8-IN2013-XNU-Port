# Revue GICv3 / timer : référence kebab vers IN2013

Date : 7 novembre 2026

## Constat dans le dépôt IN2013

Le patch `0017-sweet-device-tree-virtual-timer-irq.patch` rend l'INTID du timer virtuel configurable depuis l'ADT (valeur IN2013 : 27). Le patch `0169-instantnoodle-board-target.patch` définit déjà les constantes GICv3 SM8250 et les macros de cible INSTANTNOODLE.

Cela ne prouve pas à lui seul que le routage d'interruptions choisi par XNU correspond à l'état laissé par l'ABL.

## Ce que montre la référence OnePlus 8T

Le patch GFander `0031-gic-vtimer-kebab.patch` documente et implémente un chemin GICv3 où les interruptions non sécurisées sont reçues en Group 1 IRQ, TrustZone conservant Group 0. Il configure notamment :
- le groupe non sécurisé des PPI ;
- les registres `ICC_BPR1_EL1` et `ICC_IGRPEN1_EL1` ;
- les bits Group 1 du distributeur ;
- l'acquittement via `ICC_IAR1_EL1` / `ICC_EOIR1_EL1`.

Le patch ajoute aussi beaucoup de diagnostic spécifique KEBAB. Ces parties de diagnostic ne doivent pas être copiées telles quelles.

## Décision

**Ne pas appliquer automatiquement le patch 0031 complet.** Il modifie le chemin d'interruptions central de XNU et un mauvais choix Group 0/Group 1 peut bloquer le timer ou les IPI au démarrage.

La similitude SM8250 rend cette piste prioritaire, mais il faut d'abord comparer :
1. l'initialisation GIC réellement faite par l'ABL IN2013 ;
2. les registres GICD/GICR et ICC accessibles en non sécurisé ;
3. les changements déjà présents dans les patches 0017 et 0169 ;
4. le chemin IRQ/FIQ de la version XNU 8796.141.3 utilisée ici.

## Petit correctif transféré

Le patch `0170-xnu-native-arch-timer-guard.patch` reprend le correctif générique GFander `0008-architectural-timer-entry.patch`. Il évite que l'initialisation historique du timer BCM2837 soit activée lorsque `XNU_NATIVE_ARCH_TIMER` est activé.

Ce correctif est ajouté à la branche d'intégration, mais son application sur l'archive XNU et la compilation macOS restent à valider par CI. Il ne constitue pas une preuve de compilation réussie.

## Prochaine validation

Lancer le workflow XNU sur cette branche, vérifier que les 170 patches sont appliqués, puis examiner les erreurs de compilation. Le routage Group 1 reste séparé jusqu'à preuve suffisante sur IN2013.
