use std::env;
use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};

use bindgen::callbacks::{EnumVariantValue, ParseCallbacks};

#[derive(Debug)]
struct SiecsCallbacks;

impl ParseCallbacks for SiecsCallbacks {
    fn enum_variant_name(
        &self,
        enum_name: Option<&str>,
        original_variant_name: &str,
        _variant_value: EnumVariantValue,
    ) -> Option<String> {
        let prefix = match enum_name {
            Some("ecs_phase_t") | Some("ecs_term_access_t") => "Ecs",
            Some("ecs_field_kind_t") => "EcsField",
            _ => return None,
        };

        original_variant_name
            .strip_prefix(prefix)
            .map(str::to_owned)
    }
}

fn rust_root() -> Result<PathBuf, Box<dyn Error>> {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(2)
        .map(Path::to_path_buf)
        .ok_or_else(|| "could not locate the Rust crate root".into())
}

fn generate(header: &Path) -> Result<String, Box<dyn Error>> {
    let vendor_dir = header
        .parent()
        .ok_or("vendor header has no parent directory")?;

    let bindings = bindgen::Builder::default()
        .header(header.to_string_lossy())
        .clang_arg("-x")
        .clang_arg("c")
        .clang_arg("-std=c2x")
        .clang_arg(format!("-I{}", vendor_dir.display()))
        .allowlist_function("^ecs_.*")
        .allowlist_type("^ecs_.*|^sireflect_struct_desc_t$")
        .allowlist_var("^(Ecs|ECS)_?.*")
        .opaque_type("^sireflect_struct_desc_t$")
        .rustified_enum("^ecs_(phase|term_access|field_kind)_t$")
        .layout_tests(true)
        .use_core()
        .formatter(bindgen::Formatter::Rustfmt)
        .parse_callbacks(Box::new(SiecsCallbacks))
        .generate()
        .map_err(|error| format!("bindgen failed: {error}"))?;

    Ok(bindings.to_string())
}

fn check(output: &Path, generated: &str) -> Result<(), Box<dyn Error>> {
    let current = fs::read_to_string(output)
        .map_err(|error| format!("could not read {}: {error}", output.display()))?;

    if current == generated {
        return Ok(());
    }

    Err(format!(
        "{} is stale; run `make update-rust-vendor`",
        output.display()
    )
    .into())
}

fn write_atomic(output: &Path, generated: &str) -> Result<(), Box<dyn Error>> {
    let temporary = output.with_extension("rs.tmp");
    fs::write(&temporary, generated)?;
    fs::rename(&temporary, output)?;
    Ok(())
}

fn main() -> Result<(), Box<dyn Error>> {
    let check_only = match env::args().nth(1).as_deref() {
        None => false,
        Some("--check") => true,
        Some(argument) => return Err(format!("unknown argument: {argument}").into()),
    };

    let root = rust_root()?;
    let header = root.join("vendor/siecs.h");
    let output = root.join("src/raw/generated.rs");

    if !header.is_file() {
        return Err(format!(
            "missing {}; run `make update-rust-vendor`",
            header.display()
        )
        .into());
    }

    let generated = generate(&header)?;

    if check_only {
        check(&output, &generated)
    } else {
        write_atomic(&output, &generated)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rename(enum_name: &str, variant: &str) -> Option<String> {
        SiecsCallbacks.enum_variant_name(Some(enum_name), variant, EnumVariantValue::Signed(0))
    }

    #[test]
    fn preserves_public_rust_enum_variant_names() {
        assert_eq!(
            rename("ecs_phase_t", "EcsOnUpdate").as_deref(),
            Some("OnUpdate")
        );
        assert_eq!(
            rename("ecs_term_access_t", "EcsInOutOptional").as_deref(),
            Some("InOutOptional")
        );
        assert_eq!(
            rename("ecs_field_kind_t", "EcsFieldShared").as_deref(),
            Some("Shared")
        );
    }

    #[test]
    fn check_rejects_stale_bindings() {
        let path = env::temp_dir().join(format!("siecs-bindgen-check-{}", std::process::id()));
        fs::write(&path, "current").unwrap();

        assert!(check(&path, "current").is_ok());
        assert!(check(&path, "changed").is_err());

        fs::remove_file(path).unwrap();
    }
}
