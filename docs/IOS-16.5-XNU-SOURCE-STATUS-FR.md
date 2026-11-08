# Référence XNU exacte pour iOS 16.5 (20F66)

Date de vérification : 8 novembre 2026.

## Version du noyau cible

Pour l'identité logicielle D22AP / iPhone10,3, iOS 16.5 (build 20F66) rapporte :

- Darwin 22.5.0
- `xnu-8796.122.4~1`
- `RELEASE_ARM64_T8015`

Cette chaîne est visible dans un panic log iPhone10,3 / 20F66 et dans les références de recherche iPhone X 16.5. Elle identifie le noyau du firmware cible, pas le SoC du OnePlus.

## Ce que prouve le build IN2013 actuel

Le log du run PR 68 affiche explicitement le banner `xnu-8796.141.3`. Le workflow sélectionne cette source via le profil macOS 13.5 de `blacktop/darwin-xnu-build`.

Le run 68 prouve donc que notre configuration de bring-up IN2013 compile avec XNU 8796.141.3. Il ne prouve ni l'identité binaire avec 8796.122.4~1, ni la compatibilité du noyau 8796.141.3 avec le userland iOS 16.5.

## Disponibilité publique de la source exacte

Le 8 novembre 2026, les résolutions de refs dans `apple-oss-distributions/xnu` ont donné :

- `xnu-8796.122.4` : 404, aucun commit pour cette ref ;
- `xnu-8796.122.4~1` : 404, aucun commit pour cette ref ;
- `xnu-8796.141.3` : ref résolue.

Une lecture paginée de `git/refs/tags` sur le dépôt public `apple-oss-distributions/xnu` a retourné les trois tags de la famille 8796 suivants : `xnu-8796.101.5`, `xnu-8796.121.2` et `xnu-8796.141.3`. Aucun tag `8796.122.4` ou `8796.122.4~1` n'est publié dans cette liste. Le tag public le plus proche en numéro est donc `8796.121.2`, mais il n'est pas identique à la source iOS 16.5 demandée.

La documentation publique de recherche XNU indique aussi que les snapshots XNU spécifiques aux versions iOS ne sont pas publiés séparément comme sources iOS; Apple publie les snapshots macOS dans son dépôt OSS. Cela rend la source exacte iOS 16.5 indisponible dans le canal public Apple OSS vérifié, sans exclure une archive interne/non publique ou un miroir tiers non vérifié. Ne pas substituer arbitrairement 13.4 ou 13.5 et ne pas modifier la source de build tant qu'une source exacte ou une décision explicite de compatibilité n'est pas établie.

## ADT D22AP : identité seulement, pas matériel

L'IPSW iOS 16.5 (20F66) pour iPhone10,3/iPhone10,6 est disponible sur le CDN Apple. Le workflow tente maintenant de récupérer uniquement `DeviceTree.d22ap.im4p` par requêtes HTTP Range ; il ne télécharge pas le fichier IPSW complet de 5,7 GiB.

Après décodage IM4P, `mkadt-in2013.py` copie uniquement les propriétés `/product` du DeviceTree D22AP. Les nœuds matériels OnePlus 8 (SM8250, GIC, UFS, framebuffer, etc.) restent issus du profil IN2013. Si la récupération ou le décodage échoue, l'étape est non bloquante et l'ADT demeure synthétique sans `/product` : aucune propriété ne doit être inventée.

Aucune restauration IPSW, modification du téléphone ou opération de boot n'est effectuée.
