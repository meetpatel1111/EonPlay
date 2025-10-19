# Create third_party directory structure
$vlcIncludeDir = "third_party\vlc\include\vlc"
$vlcLibDir = "third_party\vlc\lib"

Write-Host "Creating VLC directory structure..."
New-Item -ItemType Directory -Force -Path $vlcIncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $vlcLibDir | Out-Null

Write-Host "✓ Created directories"

# Create minimal VLC header
$headerPath = Join-Path $vlcIncludeDir "vlc.h"
$headerContent = "#ifndef VLC_VLC_H`n#define VLC_VLC_H`n`n#ifdef __cplusplus`nextern `"C`" {`n#endif`n`ntypedef struct libvlc_instance_t libvlc_instance_t;`ntypedef struct libvlc_media_t libvlc_media_t;`ntypedef struct libvlc_media_player_t libvlc_media_player_t;`ntypedef long long libvlc_time_t;`ntypedef int libvlc_media_parsed_status_t;`n`nlibvlc_instance_t* libvlc_new(int argc, const char* const* argv);`nvoid libvlc_release(libvlc_instance_t* p_instance);`nlibvlc_media_t* libvlc_media_new_path(libvlc_instance_t* p_instance, const char* path);`nvoid libvlc_media_release(libvlc_media_t* p_media);`n`n#ifdef __cplusplus`n}`n#endif`n`n#endif"

$headerContent | Out-File -FilePath $headerPath -Encoding UTF8
Write-Host "✓ Created VLC header"

# Create stub libraries
$libvlcPath = Join-Path $vlcLibDir "libvlc.lib"
$libvlccorePath = Join-Path $vlcLibDir "libvlccore.lib"

"STUB" | Out-File -FilePath $libvlcPath -Encoding ASCII
"STUB" | Out-File -FilePath $libvlccorePath -Encoding ASCII

Write-Host "✓ Created stub libraries"
Write-Host "VLC setup complete!"

# Show structure
Get-ChildItem -Recurse "third_party" | Select-Object FullName