# Telecharge VB-Cable (VB-Audio, donationware) depuis le site officiel et lance son propre setup :
# l'utilisateur accepte leurs conditions, rien n'est redistribue par Gragarda Voice.
$ErrorActionPreference = 'Stop'
$zip = Join-Path $env:TEMP 'VBCABLE_Driver_Pack45.zip'
$dir = Join-Path $env:TEMP 'VBCABLE_Driver_Pack45'
Invoke-WebRequest -UseBasicParsing 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip' -OutFile $zip
Expand-Archive -Force $zip $dir
Start-Process -Wait -FilePath (Join-Path $dir 'VBCABLE_Setup_x64.exe')
