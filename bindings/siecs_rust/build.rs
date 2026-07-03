use std::fs;
use std::path::{Path, PathBuf};

fn add_c_files(build: &mut cc::Build, dir: impl AsRef<Path>) {
    let dir = dir.as_ref();
    let mut entries = fs::read_dir(dir)
        .unwrap_or_else(|err| panic!("failed to read {}: {err}", dir.display()))
        .map(|entry| entry.expect("failed to read directory entry").path())
        .collect::<Vec<_>>();

    entries.sort();

    for path in entries {
        if path.is_dir() {
            add_c_files(build, &path);
        } else if path.extension().is_some_and(|ext| ext == "c") {
            build.file(path);
        }
    }
}

fn rerun_dir(dir: impl AsRef<Path>) {
    let dir = dir.as_ref();
    let mut entries = fs::read_dir(dir)
        .unwrap_or_else(|err| panic!("failed to read {}: {err}", dir.display()))
        .map(|entry| entry.expect("failed to read directory entry").path())
        .collect::<Vec<_>>();

    entries.sort();

    for path in entries {
        if path.is_dir() {
            rerun_dir(path);
        } else {
            println!("cargo:rerun-if-changed={}", path.display());
        }
    }
}

fn main() {
    let root = PathBuf::from("vendor");

    let mut build = cc::Build::new();
    build
        .include(root.join("siecs/include"))
        .include(root.join("siecs/src"))
        .include(root.join("deps/sireflect/include"))
        .include(root.join("deps/sireflect/src"))
        .include(root.join("deps/sijson/include"))
        .include(root.join("deps/sijson/src"))
        .include(root.join("deps/sihttp/include"))
        .include(root.join("deps/sihttp/src"))
        .define("siecs_STATIC", None)
        .define("sireflect_STATIC", None)
        .define("sijson_STATIC", None)
        .define("sihttp_STATIC", None)
        .define("_POSIX_C_SOURCE", Some("200809L"))
        .flag_if_supported("-std=c2x")
        .warnings(false);

    add_c_files(&mut build, root.join("deps/sireflect/src"));
    add_c_files(&mut build, root.join("deps/sijson/src"));
    add_c_files(&mut build, root.join("deps/sihttp/src"));
    add_c_files(&mut build, root.join("siecs/src"));

    build.compile("siecs");

    rerun_dir("vendor");

    if cfg!(target_family = "unix") {
        println!("cargo:rustc-link-lib=pthread");
    }
}
