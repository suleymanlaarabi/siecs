#!/usr/bin/env sh
set -eu

install_dep() {
    id=$1
    repo=$2
    bake_home=${BAKE_HOME:-$HOME/bake}

    if [ -d "$bake_home/src/$id" ] || [ -d "$bake_home/src/$id.git" ]; then
        return
    fi

    bake clone --to-env "$repo"
}

install_dep sireflect https://github.com/suleymanlaarabi/sireflect.git
install_dep sijson https://github.com/suleymanlaarabi/sijson.git
install_dep sihttp https://github.com/suleymanlaarabi/sihttp.git
