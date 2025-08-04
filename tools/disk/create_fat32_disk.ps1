# Windows PowerShell version of FAT32 disk creation script

$IMG_FILE = "test_fat32.img"
$IMG_SIZE_MB = 100

Write-Host "Creating FAT32 disk image (Windows)..." -ForegroundColor Green

# Create empty disk image (100MB of zeros)
Write-Host "Creating $IMG_SIZE_MB MB empty image..."
$bytes = New-Object byte[] (1MB)
$stream = [System.IO.File]::Create($IMG_FILE)
for ($i = 0; $i -lt $IMG_SIZE_MB; $i++) {
    $stream.Write($bytes, 0, $bytes.Length)
    if (($i + 1) % 10 -eq 0) {
        Write-Host "  Progress: $($i + 1)/$IMG_SIZE_MB MB" -ForegroundColor Yellow
    }
}
$stream.Close()

Write-Host "Raw image created successfully!" -ForegroundColor Green

# Windows formatting approach using diskpart
Write-Host "Formatting as FAT32..." -ForegroundColor Green
Write-Host "Note: Windows FAT32 formatting requires more complex setup." -ForegroundColor Yellow
Write-Host "Alternative: Use a Linux VM or WSL for proper FAT32 formatting." -ForegroundColor Yellow

# Create diskpart script
$diskpartScript = @"
select vdisk file="$PWD\$IMG_FILE"
attach vdisk
create partition primary
select partition 1
active
format fs=fat32 label="NO_NAME" quick
assign
detach vdisk
exit
"@

$diskpartScript | Out-File -FilePath "diskpart_script.txt" -Encoding ASCII

Write-Host ""
Write-Host "To format the disk image:" -ForegroundColor Cyan
Write-Host "1. Run PowerShell as Administrator" -ForegroundColor White
Write-Host "2. Execute: diskpart /s diskpart_script.txt" -ForegroundColor White
Write-Host "3. The disk will be temporarily mounted and formatted" -ForegroundColor White

Write-Host ""
Write-Host "Image size: $([math]::Round((Get-Item $IMG_FILE).Length / 1MB, 2)) MB" -ForegroundColor Green

Write-Host ""
Write-Host "✅ Windows disk image creation setup complete!" -ForegroundColor Green
Write-Host "Note: Manual diskpart step required for FAT32 formatting" -ForegroundColor Yellow