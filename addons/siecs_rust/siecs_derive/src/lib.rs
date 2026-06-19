use proc_macro::{TokenStream, TokenTree};

#[proc_macro_derive(Component)]
pub fn derive_component(input: TokenStream) -> TokenStream {
    let name = struct_name(input).expect("Component can only be derived for structs");
    let expanded = format!(
        r#"
        impl {name} {{
            #[inline]
            pub fn id(world: &mut ::siecs::World) -> ::siecs::raw::ComponentId {{
                static mut ID: ::siecs::raw::ComponentId = 0;

                unsafe {{
                    if ID == 0 {{
                        ID = ::siecs::raw::ecs_component_init(
                            world.as_raw_mut(),
                            &::siecs::raw::ComponentDesc {{
                                name: concat!(stringify!({name}), "\0").as_ptr().cast(),
                                size: ::core::mem::size_of::<{name}>() as u64,
                                on_set: None,
                                on_remove: None,
                                on_add: None,
                                relation_flags: 0,
                                struct_desc: ::core::ptr::null(),
                            }},
                        );
                    }}

                    ID
                }}
            }}
        }}

        impl ::siecs::Component for {name} {{
            #[inline]
            fn id(world: &mut ::siecs::World) -> ::siecs::raw::ComponentId {{
                {name}::id(world)
            }}
        }}
        "#
    );

    expanded.parse().expect("failed to generate Component impl")
}

fn struct_name(input: TokenStream) -> Option<String> {
    let mut seen_struct = false;

    for token in input {
        match token {
            TokenTree::Ident(ident) if seen_struct => return Some(ident.to_string()),
            TokenTree::Ident(ident) if ident.to_string() == "struct" => seen_struct = true,
            _ => {}
        }
    }

    None
}
