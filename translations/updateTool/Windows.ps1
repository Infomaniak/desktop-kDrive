# Chemin par défaut vers lupdate.exe
$lupdatePath = "C:\Qt\Tools\QtDesignStudio\qt6_design_studio_reduced_version\bin\lupdate.exe"

# Vérifier de nouveau le chemin fourni par l'utilisateur
while (-Not (Test-Path $lupdatePath)) {
    Write-Host "Le chemin fourni n'existe pas."
    $lupdatePath = Read-Host "Veuillez fournir le chemin correct vers lupdate.exe"
}

# Trouver tous les fichiers .ts dans le dossier transalations
$tsFiles = Get-ChildItem -Path ../ -Filter "*.ts" -Recurse

# Chemin vers le dossier src
$srcPath = "..\..\src"

# Compteur de fichiers mis à jour
$updatedCount = 0

# Exécuter lupdate.exe pour chaque fichier .ts
foreach ($file in $tsFiles) {
    & $lupdatePath $srcPath -ts $file.FullName -noobsolete
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Erreur lors de l'exécution de lupdate.exe pour $($file.FullName)"
    } else {
        Write-Host "Mis a jour reussi pour $($file.FullName)"
        $updatedCount++
    }
}

# Afficher le nombre de fichiers mis à jour
Write-Host "Nombre total de fichiers mis a jour : $updatedCount"
