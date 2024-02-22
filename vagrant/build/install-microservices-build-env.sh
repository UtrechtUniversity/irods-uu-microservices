#!/bin/bash
# shellcheck disable=SC1090

set -e
set -o pipefail
set -u

export DEBIAN_FRONTEND=noninteractive

SETTINGSFILE=${1:-./local-repo.env}

if [ -f "$SETTINGSFILE" ]
then source "$SETTINGSFILE"
else echo "Error: settings file $SETTINGSFILE not found." && exit 1
fi

if [ -f /etc/centos-release ]
then

  echo "Installing iRODS UU microservices build environment on CentOS."

  echo "Installing dependencies ..."
  sudo yum -y install wget epel-release yum-plugin-versionlock

  echo "Adding iRODS repository ..."
  sudo rpm --import https://packages.irods.org/irods-signing-key.asc
  wget -qO - https://packages.irods.org/renci-irods.yum.repo | sudo tee /etc/yum.repos.d/renci-irods.yum.repo

  for package in $YUM_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo yum -y install "$package-${IRODS_VERSION}"
     sudo yum versionlock "$package"
  done

  for package in $YUM_IRODS_EXTERNAL_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo yum -y install "$package"
  done

  for package in $YUM_GEN_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo yum -y install "$package"
  done

elif lsb_release -i | grep -q Ubuntu
then

  echo "Installing iRODS UU microservices build environment on Ubuntu."

  echo "Downloading and installing iRODS repository signing key ..."
  wget -qO - "$APT_IRODS_REPO_SIGNING_KEY_LOC" | sudo apt-key add -

  echo "Adding iRODS repository ..."
cat << ENDAPTREPO | sudo tee /etc/apt/sources.list.d/irods.list
deb [arch=${APT_IRODS_REPO_ARCHITECTURE}] $APT_IRODS_REPO_URL $APT_IRODS_REPO_DISTRIBUTION $APT_IRODS_REPO_COMPONENT
ENDAPTREPO
  sudo apt update

  echo "Installing dependencies ..."
  sudo apt-get -y install aptitude libboost-locale-dev

  sudo apt install -y libpython2-stdlib libpython2.7-minimal libpython2.7-stdlib \
          python-is-python2 python-six python2 python2-minimal python2.7 python2.7-minimal \
          python-certifi python-chardet python-idna python-pkg-resources python-setuptools

  PY_URLLIB_PREFIX="http://security.ubuntu.com/ubuntu/pool/main/p/python-urllib3"
  PY_URLLIB_FILENAME="python-urllib3_1.22-1ubuntu0.18.04.2_all.deb"
  PY_REQUESTS_PREFIX="http://security.ubuntu.com/ubuntu/pool/main/r/requests"
  PY_REQUESTS_FILENAME="python-requests_2.18.4-2ubuntu0.1_all.deb"
  OPENSSL_PREFIX="http://security.ubuntu.com/ubuntu/pool/main/o/openssl1.0"
  OPENSSL_FILENAME="libssl1.0.0_1.0.2n-1ubuntu5.13_amd64.deb"

  wget \
        ${PY_URLLIB_PREFIX}/${PY_URLLIB_FILENAME} \
        ${PY_REQUESTS_PREFIX}/${PY_REQUESTS_FILENAME} \
        ${OPENSSL_PREFIX}/${OPENSSL_FILENAME}

  for package in $PY_URLLIB_FILENAME $PY_REQUESTS_FILENAME $OPENSSL_FILENAME
  do echo "Installing package ${package%.*}"
     sudo dpkg -i "$package"
     rm "$package"
  done

  for package in $APT_IRODS_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo apt-get -y install "$package=${IRODS_VERSION}-1~bionic"
     sudo aptitude hold "$package"
  done

  for package in $APT_IRODS_EXTERNAL_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo apt-get -y install "$package"
  done

  for package in $APT_GEN_PACKAGES
  do echo "Installing package $package and its dependencies"
     sudo apt-get -y install "$package"
  done

else
  echo "Error: install script is not suitable for this box."

fi

git clone https://github.com/UtrechtUniversity/irods-uu-microservices.git
chown -R vagrant:vagrant irods-uu-microservices
