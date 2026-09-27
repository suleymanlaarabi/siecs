#include <siui.h>
#include <sigpu.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    ecs_init();
    ECS_MODULE_IMPORT(sigpu,{.title="SIUI example",.width=960,.height=640,.samples=1});
    ECS_MODULE_IMPORT(siui,{.gpu=true});
    ecs_entity_t root=ecs_new();
    ecs_set(root,UiNode,{
        .width=siui_percent(100),.height=siui_percent(100),
        .padding=siui_all(32),.direction=SIUI_COLUMN,.gap=16,
    });
    ecs_entity_t card=ecs_new();
    ecs_set(card,UiNode,{
        .max_width=siui_px(480),.padding=siui_all(20),
        .border_width=siui_all(2),.direction=SIUI_COLUMN,.gap=8,
    });
    ecs_set(card,UiPaint,{
        .background={24,34,54,255},
        .border_top={100,130,190,255},.border_right={100,130,190,255},
        .border_bottom={100,130,190,255},.border_left={100,130,190,255},
    });
    ecs_relate(card,ChildOf,root);
    const char *message="SIUI overlay — Hello world";
    siui_set_text(card,message,strlen(message));
    ecs_entity_t strip=ecs_new();
    ecs_set(strip,UiNode,{.width=siui_px(440),.height=siui_px(48)});
    ecs_set(strip,UiPaint,{.background={65,105,180,220}});
    ecs_relate(strip,ChildOf,root);
    const char *limit=getenv("SIUI_EXAMPLE_FRAMES");
    int frames=limit?atoi(limit):0;
    while(ecs_progress() && (frames==0 || --frames>0)) {}
    ecs_fini();
    return 0;
}
