#include <err.h>
#include <reg.h>
#include <dev/fbcon.h>
#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <mipi_dsi.h>
#include <platform.h>

#if defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP3)
#include <mdp3.h>
#elif defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP4)
#include <mdp4.h>
#elif defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP5)
#include <mdp5.h>
#else
#error "MDP is not supported by this platform"
#endif

#include <platform/iomap.h>
#include <dev/lcdc.h>

int check_aboot_addr_range_overlap(uint32_t start, uint32_t size);

#if defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP3)
static int mdp_dump_config(struct fbcon_config *fb)
{
    fb->base = (void *) readl(MDP_DMA_P_BUF_ADDR);
    fb->width = readl(MDP_DMA_P_BUF_Y_STRIDE)/3;
    fb->height = readl(MDP_DMA_P_SIZE)>>16;
    fb->stride = fb->width;
    fb->bpp = 24;
    fb->format = FB_FORMAT_RGB888;

    return NO_ERROR;
}

#elif defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP4)
static int mdp_dump_config(struct fbcon_config *fb)
{
    fb->base = (void *) readl(MDP_DMA_P_BUF_ADDR);
    fb->width = readl(MDP_DMA_P_BUF_Y_STRIDE)/3;
    fb->height = readl(MDP_DMA_P_SIZE)>>16;
    fb->stride = fb->width;
    fb->bpp = 24;
    fb->format = FB_FORMAT_RGB888;

    uint32_t trigger_ctrl = readl(DSI_TRIG_CTRL);
    int te_sel = (trigger_ctrl >> 31) & 1;
    int mdp_trigger = (trigger_ctrl >> 4) & 4;
    int dma_trigger = (trigger_ctrl) & 4;

    dprintf(INFO, "%s: trigger_ctrl=0x%x te_sel=%d mdp_trigger=%d dma_trigger=%d\n",
            __func__, trigger_ctrl, te_sel, mdp_trigger, dma_trigger);

#ifdef DISPLAY_RGBSWAP
    unsigned char DST_FORMAT = 8;
    int data = 0x00100000;
    data |= ((DISPLAY_RGBSWAP & 0x07) << 16);
    writel(data | DST_FORMAT, DSI_COMMAND_MODE_MDP_CTRL);
#endif

    if (mdp_trigger == DSI_CMD_TRIGGER_SW) {
        fb->update_start = mipi_update_sw_trigger;
    }

    return NO_ERROR;
}

#elif defined(WITH_LIB_2NDSTAGE_DISPLAY_MDP5)
static int mdp_dump_config(struct fbcon_config *fb)
{
    uint32_t pipe_base = MDP_VP_0_RGB_0_BASE;

    fb->base = (void *) readl(pipe_base + PIPE_SSPP_SRC0_ADDR);
    fb->width = readl(pipe_base + PIPE_SSPP_SRC_YSTRIDE)/3;
    fb->height = readl(pipe_base + PIPE_SSPP_SRC_IMG_SIZE)>>16;
    fb->stride = fb->width;
    fb->bpp = 24;
    fb->format = FB_FORMAT_RGB888;

    return NO_ERROR;
}
#endif

static struct fbcon_config config;

void target_display_init(const char *panel_name)
{
    int rc;

    // clear config
    memset(&config, 0, sizeof(config));

    // dump config
    rc = mdp_dump_config(&config);

    // dump failed
    if (rc || config.base==NULL || config.width==0 || config.height==0 ||
            check_aboot_addr_range_overlap((uint32_t)config.base, config.width*config.height*config.bpp/3)) {
        dprintf(CRITICAL, "Invalid config dumped: base=%p size=%ux%ux%u\n", config.base, config.width, config.height, config.bpp);
        return;
    }

    // setup fbcon
    fbcon_setup(&config);

    // display logo
    display_image_on_screen();
}
