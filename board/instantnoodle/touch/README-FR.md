# IN2013 XNU HID Bridge — v5

Cette étape ajoute une couche d'orchestration portable entre le cœur S6SY771 v4 et les futurs callbacks de carte/XNU.

## Tests Windows

À lancer dans Developer PowerShell for Visual Studio, après chargement de l'environnement MSVC et ajout de `C:\Program Files\LLVM\bin` au PATH :

```powershell
cd C:\oneplus8_ios_port_pack\IN2013-XNU-HID-Bridge-v5
Set-ExecutionPolicy -Scope Process Bypass -Force
Unblock-File .\tests\run-tests.ps1
.\tests\run-tests.ps1
```

Résultat attendu : `S6SY771 board/HID bridge tests: 4/4 checks passed`.

## Important

Ce paquet n'est pas un pilote IOKit complet et ne doit pas être copié dans XNU en prétendant être prêt pour le matériel. Le rapport `docs/AUDIT-APOLLO-ET-PLAN-XNU-FR.md` décrit les éléments confirmés et les blocages. Aucun accès au téléphone n'est requis.
