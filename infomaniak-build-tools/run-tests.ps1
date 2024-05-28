$testers = Get-ChildItem build-windows -Recurse -Name -Filter 'kDrive_test_*.exe'
$errors = 0

pushd build-windows\install\bin

foreach ($file in $testers)
{
    $path="..\..\$file"
    & $path
    $errors += $LASTEXITCODE
}

popd
exit $errors
