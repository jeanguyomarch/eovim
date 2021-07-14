#! /usr/bin/env sh

set -e

if [ "$LIBDIR" = "" ]; then
  LIBDIR="/usr/lib"
fi

set -u

if [ $# -ne 2 ]; then
  echo "Usage: $0 <AppDir> <output.appimage>" 1>&2
  exit 1
fi

APPDIR="$(realpath $1)"
OUTPUT="$2"

if [ ! -d "$APPDIR" ]; then
  echo "E: Please provide the initial AppDir (ie. make install)" 1>&2
  exit 1
fi

#==============================================================================#
#                            Package the EFL Modules                           #
#==============================================================================#

EFL_VERSION=$(pkg-config --modversion elementary)

echo "Found EFL version $EFL_VERSION"

EFL_MODULES="
ecore
ecore_con
ecore_evas
ecore_imf
ecore_wl2
edje
eeze
efreet
elementary
emotion
evas
"

for efl_mod in $EFL_MODULES; do
  dir="$LIBDIR/$efl_mod"
  if [ ! -d "$dir" ]; then
    echo "E: failed to find '$dir'. You may change the library directory prefix by setting the environment variable 'LIBDIR'" 1>&2
    exit 1
  fi

  cd "$dir"
  shared_objects=$(find . -type f -name "*.so" -print)
  for shared_object in $shared_objects; do
    # FIXME - This is probably a very bad practice to hardcode usr/lib in the path.
    # This is easier for me now, but it will surely fail one day... sorry future
    # me...
    dir2=usr/lib/$efl_mod/$(dirname "$shared_object")
    mkdir -p "$APPDIR/$dir2"
    cp "$shared_object" "$APPDIR/$dir2"
  done
  cd - > /dev/null
done


#==============================================================================#
#                                  LinuxDeploy                                 #
#==============================================================================#

LINUXDEPLOY=linuxdeploy-x86_64.AppImage
if [ ! -f "$LINUXDEPLOY" ]; then
  wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$LINUXDEPLOY"
  chmod +x "$LINUXDEPLOY"
  LINUXDEPLOY="./$LINUXDEPLOY"
fi

export OUTPUT
export UPDATE_INFORMATION='gh-releases-zsync|jeanguyomarch|eovim|nightly|*.AppImage.zsync'
./"$LINUXDEPLOY" --appdir "$APPDIR" -d "$APPDIR/usr/share/applications/eovim.desktop" -i "$APPDIR/usr/share/icons/eovim.png" --output appimage

ls *
