#! /usr/bin/env sh

set -e

if [ "$LIBDIR" = "" ]; then
  LIBDIR="/usr/lib"
fi

if [ "$SHAREDIR" = "" ]; then
  SHAREDIR="/usr/share"
fi

set -u

if [ $# -ne 3 ]; then
  echo "Usage: $0 <AppDir> <output.appimage> <release tag>" 1>&2
  exit 1
fi

APPDIR="$(realpath $1)"
OUTPUT="$2"
TAG="$3"

if [ ! -d "$APPDIR" ]; then
  echo "E: Please provide the initial AppDir (ie. make install)" 1>&2
  exit 1
fi

EFL_VERSION=$(pkg-config --modversion elementary)
echo "Found EFL version $EFL_VERSION"

#==============================================================================#
#                            Package the EFL Modules                           #
#==============================================================================#

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
    dest_dir=usr/lib/$efl_mod/$(dirname "$shared_object")
    mkdir -p "$APPDIR/$dest_dir"
    name=$(basename "$shared_object")
    so_file="$APPDIR/$dest_dir/$name"
    cp "$shared_object" "$so_file"

    rel=$(realpath --relative-to="$so_file" "$APPDIR/usr/lib")
    patchelf --set-rpath "\$ORIGIN/$rel" "$so_file"
    echo "Patching RPATH of shared object $so_file to '\$ORIGIN/$rel'"
  done
  cd - > /dev/null
done


#==============================================================================#
#                             Package the EFL data                             #
#==============================================================================#

EFL_DATA_DIRS="
elementary
ethumb
evas
"

for efl_data_dir in $EFL_DATA_DIRS; do
  dir="$SHAREDIR/$efl_data_dir"
  if [ ! -d "$dir" ]; then
    echo "E: failed to find '$dir'. You may change the data directory prefix by setting the environment variable 'SHAREDIR'" 1>&2
    exit 1
  fi
  rm -rf "$APPDIR/usr/share/$efl_data_dir"
  cp -r "$dir" "$APPDIR/usr/share/$efl_data_dir"
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
#export UPDATE_INFORMATION="gh-releases-zsync|jeanguyomarch|eovim|$TAG|$OUTPUT.zsync"
./"$LINUXDEPLOY" --appdir "$APPDIR" -d "$APPDIR/usr/share/applications/eovim.desktop" -i "$APPDIR/usr/share/icons/eovim.png" --output appimage

ls *
tree "$APPDIR"
du -sh "$APPDIR"