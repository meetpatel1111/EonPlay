Name:           eonplay
Version:        1.0.0
Release:        1%{?dist}
Summary:        Timeless, futuristic media player - play for eons
License:        GPL-3.0+
URL:            https://eonplay.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2.0
BuildRequires:  qt6-qtmultimedia-devel >= 6.2.0
BuildRequires:  qt6-qtnetworkauth-devel >= 6.2.0
BuildRequires:  vlc-devel >= 3.0.0
BuildRequires:  libva-devel
BuildRequires:  libvdpau-devel
BuildRequires:  desktop-file-utils
BuildRequires:  appstream-util

Requires:       qt6-qtbase >= 6.2.0
Requires:       qt6-qtmultimedia >= 6.2.0
Requires:       vlc-core >= 3.0.0
Requires:       libva
Requires:       libvdpau
Requires:       hicolor-icon-theme

Recommends:     vlc-extras
Recommends:     ffmpeg-libs

%description
EonPlay is a comprehensive, cross-platform media player that delivers
a cutting-edge media experience with advanced features and comprehensive
format support.

Key Features:
* Support for all major audio and video formats
* Hardware-accelerated playback (VA-API/VDPAU)
* Advanced audio processing with equalizer and effects
* Comprehensive subtitle support with AI features
* Network streaming and casting capabilities
* Library management with smart playlists
* Security features and parental controls
* Modern, futuristic user interface

EonPlay uses libVLC for robust media playback and Qt 6 for a native,
responsive user interface that integrates seamlessly with your desktop
environment.

%prep
%autosetup

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF
%cmake_build

%install
%cmake_install

# Install desktop file
desktop-file-install \
    --dir=%{buildroot}%{_datadir}/applications \
    packaging/linux/eonplay.desktop

# Install icon
install -Dm644 resources/icons/app_icon.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/eonplay.png

# Install MIME type associations
install -Dm644 packaging/linux/eonplay-mimetypes.xml \
    %{buildroot}%{_datadir}/mime/packages/eonplay.xml

# Install AppStream metadata
install -Dm644 packaging/linux/eonplay.appdata.xml \
    %{buildroot}%{_datadir}/metainfo/eonplay.appdata.xml

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/eonplay.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/eonplay.appdata.xml

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-mime-database %{_datadir}/mime &> /dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
    /usr/bin/update-mime-database %{_datadir}/mime &> /dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%files
%license LICENSE
%doc README.md
%{_bindir}/eonplay
%{_datadir}/applications/eonplay.desktop
%{_datadir}/icons/hicolor/256x256/apps/eonplay.png
%{_datadir}/mime/packages/eonplay.xml
%{_datadir}/metainfo/eonplay.appdata.xml

%changelog
* Mon Oct 20 2025 EonPlay Team <support@eonplay.com> - 1.0.0-1
- Initial release of EonPlay media player
- Comprehensive media format support
- Hardware acceleration support
- Advanced audio and video processing
- Network streaming capabilities
- Security and parental control features