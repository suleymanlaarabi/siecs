#!/usr/bin/env sh
set -eu

install_dep() {
    id=$1
    repo=$2

    if [ -d "$HOME/bake/src/$id" ] || [ -d "$HOME/bake/src/$id.git" ]; then
        return
    fi

    bake clone --to-env "$repo"
}

install_dep sireflect https://github.com/suleymanlaarabi/sireflect.git
install_dep sijson https://github.com/suleymanlaarabi/sijson.git
install_dep sihttp https://github.com/suleymanlaarabi/sihttp.git
