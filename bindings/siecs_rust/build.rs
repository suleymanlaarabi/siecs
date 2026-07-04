use std::path::Path;

fn main() {
    let c_file = Path::new("vendor/siecs.c");
    let h_file = Path::new("vendor/siecs.h");

    if !c_file.exists() || !h_file.exists() {
        panic!("missing vendor/siecs.c or vendor/siecs.h; run `make update-rust-vendor` first");
    }

    println!("cargo:rerun-if-changed={}", c_file.display());
    println!("cargo:rerun-if-changed={}", h_file.display());

    let mut build = cc::Build::new();
    build
        .file(c_file)
        .include("vendor")
        .define("siecs_STATIC", None)
        .define("_POSIX_C_SOURCE", Some("200809L"))
        .flag_if_supported("-std=c2x")
        .warnings(false);

    build.compile("siecs");

    if cfg!(target_family = "unix") {
        println!("cargo:rustc-link-lib=pthread");
    }
}
