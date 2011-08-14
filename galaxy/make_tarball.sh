#! /bin/sh
#
# Build the galaxy tarball to deploy memcached in galaxy.
#

SCRIPTS=/tmp/tc-galaxy-6.2.tar.gz

if [ -z "$SCRIPTS" -o ! -f $SCRIPTS ]; then
  echo "$SCRIPTS does not exist. You need the galaxy scripts to build the tarball!"
  exit 1
fi

if [ ! -f ../memcached ]; then
  echo "No memcache binary found. Please compile memcached first!"
  exit 1
fi

WORK_DIR=`mktemp -d`

if [ -z "$WORK_DIR" -o ! -d "$WORK_DIR" ]; then
  echo "Problems with the work dir. Bailing out!"
  exit 1
fi

PLATFORM=unknown
grep -q -i 'ubuntu' /etc/issue 
if [ "$?" = "0" ]; then
  PLATFORM=ubuntu
fi

grep -q -i 'fedora' /etc/issue 
if [ "$?" = "0" ]; then
  PLATFORM=redhat
fi
grep -q -i 'centos' /etc/issue 
if [ "$?" = "0" ]; then
  PLATFORM=redhat
fi

chmod 755 $WORK_DIR
cp ../memcached $WORK_DIR
strip $WORK_DIR/memcached
chmod 755 $WORK_DIR/memcached

mkdir -p "$WORK_DIR/bin"
tar -C "$WORK_DIR/bin" -xzf $SCRIPTS
cp launcher.memcached LAUNCHER_TYPE "$WORK_DIR/bin"
chmod -R 755 $WORK_DIR/bin

eval `grep PACKAGE_VERSION ../Makefile | sed -e 's/ //g'`

TAR_NAME=memcached-$PLATFORM-$PACKAGE_VERSION.tar.gz

tar -C $WORK_DIR -czf $TAR_NAME  memcached bin

rm -rf $WORK_DIR

echo "Created $TAR_NAME tarball."


