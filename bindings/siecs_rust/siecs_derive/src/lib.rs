use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, Attribute, DeriveInput, Expr, LitStr, Path};

#[proc_macro_derive(Component, attributes(component))]
pub fn derive_component(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let ident = input.ident;
    let attrs = ComponentAttrs::parse(&input.attrs);
    let name = attrs.name.unwrap_or_else(|| ident.to_string());
    let on_add = option_path(attrs.on_add);
    let on_set = option_path(attrs.on_set);
    let on_remove = option_path(attrs.on_remove);
    let struct_desc = attrs
        .struct_desc
        .map(|path| quote! { #path })
        .unwrap_or_else(|| quote! { ::core::ptr::null() });
    let relation_flags = attrs.relation_flags;

    quote! {
        impl #ident {
            #[inline]
            pub fn id(world: &mut ::siecs::World) -> ::siecs::raw::ComponentId {
                <Self as ::siecs::Component>::id(world)
            }
        }

        impl ::siecs::Component for #ident {
            #[inline]
            unsafe fn id_raw(world: *mut ::siecs::raw::WorldRaw) -> ::siecs::raw::ComponentId {
                static ID: ::siecs::private::StaticComponentId =
                    ::siecs::private::StaticComponentId::new();

                ::siecs::raw::ecs_component_register(
                    world,
                    ID.as_mut_ptr(),
                    &::siecs::raw::ComponentDesc {
                        name: concat!(#name, "\0").as_ptr().cast(),
                        size: ::core::mem::size_of::<#ident>() as u64,
                        on_set: #on_set,
                        on_remove: #on_remove,
                        on_add: #on_add,
                        relation_flags: #relation_flags,
                        struct_desc: #struct_desc,
                    },
                )
            }
        }
    }
    .into()
}

#[proc_macro_derive(Resource, attributes(resource))]
pub fn derive_resource(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let ident = input.ident;
    let attrs = ResourceAttrs::parse(&input.attrs);
    let name = attrs.name.unwrap_or_else(|| ident.to_string());
    let on_set = option_path(attrs.on_set);
    let on_remove = option_path(attrs.on_remove);

    quote! {
        impl #ident {
            #[inline]
            pub fn id(world: &mut ::siecs::World) -> ::siecs::raw::ResourceId {
                <Self as ::siecs::Resource>::id(world)
            }
        }

        impl ::siecs::Resource for #ident {
            #[inline]
            unsafe fn id_raw(world: *mut ::siecs::raw::WorldRaw) -> ::siecs::raw::ResourceId {
                static ID: ::siecs::private::StaticResourceId =
                    ::siecs::private::StaticResourceId::new();

                ::siecs::raw::ecs_resource_register(
                    world,
                    ID.as_mut_ptr(),
                    &::siecs::raw::ResourceDesc {
                        name: concat!(#name, "\0").as_ptr().cast(),
                        size: ::core::mem::size_of::<#ident>() as u64,
                        on_set: #on_set,
                        on_remove: #on_remove,
                    },
                )
            }
        }
    }
    .into()
}

#[derive(Default)]
struct ComponentAttrs {
    name: Option<String>,
    relation_flags: u32,
    on_add: Option<Path>,
    on_set: Option<Path>,
    on_remove: Option<Path>,
    struct_desc: Option<Expr>,
}

impl ComponentAttrs {
    fn parse(attrs: &[Attribute]) -> Self {
        let mut out = Self::default();

        for attr in attrs.iter().filter(|attr| attr.path().is_ident("component")) {
            attr.parse_nested_meta(|meta| {
                if meta.path.is_ident("name") {
                    let value = meta.value()?.parse::<LitStr>()?;
                    out.name = Some(value.value());
                    return Ok(());
                }
                if meta.path.is_ident("on_add") {
                    out.on_add = Some(meta.value()?.parse()?);
                    return Ok(());
                }
                if meta.path.is_ident("on_set") {
                    out.on_set = Some(meta.value()?.parse()?);
                    return Ok(());
                }
                if meta.path.is_ident("on_remove") {
                    out.on_remove = Some(meta.value()?.parse()?);
                    return Ok(());
                }
                if meta.path.is_ident("struct_desc") {
                    out.struct_desc = Some(meta.value()?.parse()?);
                    return Ok(());
                }
                if meta.path.is_ident("relation") {
                    out.relation_flags |= quote_flag("ECS_RELATION_TARGET");
                    meta.parse_nested_meta(|flag| {
                        if flag.path.is_ident("cascade_delete") {
                            out.relation_flags |= quote_flag("ECS_RELATION_CASCADE_DELETE");
                        } else if flag.path.is_ident("one_to_one") {
                            out.relation_flags |= quote_flag("ECS_RELATION_ONE_TO_ONE");
                        } else if flag.path.is_ident("one_to_many") {
                            out.relation_flags |= quote_flag("ECS_RELATION_ONE_TO_MANY");
                        } else {
                            return Err(flag.error("unsupported relation flag"));
                        }
                        Ok(())
                    })?;
                    return Ok(());
                }

                Err(meta.error("unsupported component attribute"))
            })
            .expect("invalid component attribute");
        }

        out
    }
}

#[derive(Default)]
struct ResourceAttrs {
    name: Option<String>,
    on_set: Option<Path>,
    on_remove: Option<Path>,
}

impl ResourceAttrs {
    fn parse(attrs: &[Attribute]) -> Self {
        let mut out = Self::default();

        for attr in attrs.iter().filter(|attr| attr.path().is_ident("resource")) {
            attr.parse_nested_meta(|meta| {
                if meta.path.is_ident("name") {
                    let value = meta.value()?.parse::<LitStr>()?;
                    out.name = Some(value.value());
                    return Ok(());
                }
                if meta.path.is_ident("on_set") {
                    out.on_set = Some(meta.value()?.parse()?);
                    return Ok(());
                }
                if meta.path.is_ident("on_remove") {
                    out.on_remove = Some(meta.value()?.parse()?);
                    return Ok(());
                }

                Err(meta.error("unsupported resource attribute"))
            })
            .expect("invalid resource attribute");
        }

        out
    }
}

fn option_path(path: Option<Path>) -> proc_macro2::TokenStream {
    path.map(|path| quote! { Some(#path) })
        .unwrap_or_else(|| quote! { None })
}

fn quote_flag(name: &str) -> u32 {
    match name {
        "ECS_RELATION_TARGET" => 1 << 0,
        "ECS_RELATION_CASCADE_DELETE" => 1 << 2,
        "ECS_RELATION_ONE_TO_ONE" => 1 << 3,
        "ECS_RELATION_ONE_TO_MANY" => 1 << 4,
        _ => 0,
    }
}
