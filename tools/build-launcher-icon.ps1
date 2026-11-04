param([string]$Source,[string]$Destination)
$ErrorActionPreference='Stop'
if(-not $Source){$Source=Join-Path $PSScriptRoot '../src/launcher/assets/launcher-a-source.png'}
if(-not $Destination){$Destination=Join-Path $PSScriptRoot '../src/launcher/assets/launcher.ico'}
Add-Type -AssemblyName System.Drawing
$sourceImage=[Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Source).Path)
$frames=[Collections.Generic.List[object]]::new()
$sizes=@(16,20,24,32,40,48,64,128,256)
try {
 foreach($size in $sizes){
  $bitmap=[Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try {
   $graphics=[Drawing.Graphics]::FromImage($bitmap)
   try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.CompositingQuality=[Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    # Fit uniformly; never stretch the letter or remove its transparent margins.
    $scale=[Math]::Min($size/$sourceImage.Width,$size/$sourceImage.Height)
    $width=[float]($sourceImage.Width*$scale);$height=[float]($sourceImage.Height*$scale)
    $rect=[Drawing.RectangleF]::new(($size-$width)/2,($size-$height)/2,$width,$height)
    $graphics.DrawImage($sourceImage,$rect)
   } finally {$graphics.Dispose()}
   $stream=[IO.MemoryStream]::new()
   try {$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png);$frames.Add($stream.ToArray())}
   finally {$stream.Dispose()}
   if($size -eq 256){$bitmap.Save((Join-Path (Split-Path $Destination) 'launcher-a-preview.png'),[Drawing.Imaging.ImageFormat]::Png)}
  } finally {$bitmap.Dispose()}
 }
 $file=[IO.File]::Create([IO.Path]::GetFullPath($Destination))
 $writer=[IO.BinaryWriter]::new($file)
 try {
  $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
  $offset=6+16*$sizes.Count
  for($i=0;$i -lt $sizes.Count;$i++){
   $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
   $writer.Write([byte]$dimension);$writer.Write([byte]$dimension)
   $writer.Write([byte]0);$writer.Write([byte]0)
   $writer.Write([uint16]1);$writer.Write([uint16]32)
   $writer.Write([uint32]$frames[$i].Length);$writer.Write([uint32]$offset)
   $offset+=$frames[$i].Length
  }
  foreach($frame in $frames){$writer.Write([byte[]]$frame)}
 } finally {$writer.Dispose();$file.Dispose()}
} finally {$sourceImage.Dispose()}
Write-Output ('Launcher A icon packaged: '+$Destination)
