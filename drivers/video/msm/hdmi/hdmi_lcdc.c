/* drivers/video/msm/hdmi_lcdc.c
 *
 * Copyright (c) 2009 Google Inc.
 * Copyright (c) 2009 HTC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <linux/io.h>
#include <asm/mach-types.h>

#include <mach/msm_fb.h>

#include "../mdp_hw.h"
#include "../../../../arch/arm/mach-msm/proc_comm.h"
#include "../../../../arch/arm/mach-msm/clock-pcom.h"

#if 1
#define HDMI_DBG(s...) printk("[hdmi/lcdc]" s)
#else
#define HDMI_DBG(s...) do {} while (0)
#endif

struct mdp_lcdc_info *_lcdc;

static struct mdp_device *mdp_dev;

#define panel_to_lcdc(p) container_of((p), struct mdp_lcdc_info, fb_panel_data)

/* FIXME: arrange the clock manipulating to proper place,
	  integrate with the counter of fb_hdmi
*/
int lcdc_enable_video(void)
{
        //struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
        struct mdp_lcdc_info *lcdc = _lcdc;
        struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

	mutex_lock(&lcdc->blank_lock);
	if (atomic_read(&lcdc->blank_count))
		goto end_enable_video;
        HDMI_DBG("%s: enable clocks\n", __func__);
        clk_enable(lcdc->mdp_clk);
        clk_enable(lcdc->pclk);
        clk_enable(lcdc->pad_pclk);

	/* TODO: need pre-test to see if it make any influence to HDCP,
	 * if ebi1_clk doesn't enabled here.
	 */
	//panel_ops->unblank(panel_ops);

        mdp_writel(lcdc->mdp, 1, MDP_LCDC_EN);
        atomic_inc(&lcdc->blank_count);
        HDMI_DBG("%s, blank_count=%d\n", __func__,
		atomic_read(&lcdc->blank_count));
end_enable_video:
	mutex_unlock(&lcdc->blank_lock);

        return 0;
}

int lcdc_disable_video(void)
{
        struct mdp_lcdc_info *lcdc = _lcdc;
        struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

	mutex_lock(&lcdc->blank_lock);
	if (atomic_read(&lcdc->blank_count) == 0)
		goto disable_video_done;
	if (atomic_dec_return(&lcdc->blank_count) == 0) {
		HDMI_DBG("%s: disable clocks\n", __func__);
		panel_ops->blank(panel_ops);
		mdp_writel(lcdc->mdp, 0, MDP_LCDC_EN);
		clk_disable(lcdc->pclk);
		clk_disable(lcdc->pad_pclk);
		clk_disable(lcdc->mdp_clk);
	}
disable_video_done:
	mutex_unlock(&lcdc->blank_lock);
	HDMI_DBG("%s, blank_count=%d\n", __func__,
			atomic_read(&lcdc->blank_count));
	return 0;
}

static int lcdc_unblank(struct msm_panel_data *fb_panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

	HDMI_DBG("%s\n", __func__);

#if 0
	HDMI_DBG("%s: enable clocks\n", __func__);
	clk_enable(lcdc->mdp_clk);
	clk_enable(lcdc->pclk);
	clk_enable(lcdc->pad_pclk);

	panel_ops->unblank(panel_ops);

	mdp_writel(lcdc->mdp, 1, MDP_LCDC_EN);
	atomic_set(&lcdc->blank_count, 1);
#else
	lcdc_enable_video();
	/* TODO: need pre-test to see if it make any influence to HDCP,
	 * if ebi1_clk enabled here.
	 */
        panel_ops->unblank(panel_ops);
#endif
	return 0;
}

static int lcdc_blank(struct msm_panel_data *fb_panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	//struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

#if 0
	mutex_lock(&lcdc->blank_lock);
	if (atomic_read(&lcdc->blank_count) == 0)
		goto blank_done;
	if (atomic_dec_return(&lcdc->blank_count) == 0) {
		HDMI_DBG("%s: disable clocks\n", __func__);
		panel_ops->blank(panel_ops);
		mdp_writel(lcdc->mdp, 0, MDP_LCDC_EN);
		clk_disable(lcdc->pclk);
		clk_disable(lcdc->pad_pclk);
		clk_disable(lcdc->mdp_clk);
	}
blank_done:
	mutex_unlock(&lcdc->blank_lock);
	HDMI_DBG("%s, blank_count=%d\n", __func__,
		atomic_read(&lcdc->blank_count));
#else
	lcdc_disable_video();
#endif
	return 0;
}

static int lcdc_suspend(struct msm_panel_data *fb_panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

	//pr_info("%s: suspending\n", __func__);
	HDMI_DBG("%s\n", __func__);

	if (panel_ops->uninit)
		panel_ops->uninit(panel_ops);
	lcdc_disable_video();

	return 0;
}

static int lcdc_resume(struct msm_panel_data *fb_panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	struct msm_lcdc_panel_ops *panel_ops = lcdc->pdata->panel_ops;

	//pr_info("%s: resuming\n", __func__);
	HDMI_DBG("%s\n", __func__);

	if (panel_ops->init) {
		if (panel_ops->init(panel_ops) < 0)
			printk(KERN_ERR "LCD init fail!\n");
	}

	return 0;
}

static int
lcdc_adjust_timing(struct msm_panel_data *fb_panel,
		struct msm_lcdc_timing *timing, u32 xres, u32 yres)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	unsigned int hsync_period;
	unsigned int hsync_start_x;
	unsigned int hsync_end_x;
	unsigned int vsync_period;
	unsigned int display_vstart;
	unsigned int display_vend;
	uint32_t dma_cfg;

	clk_set_rate(lcdc->pclk, timing->clk_rate);
	clk_set_rate(lcdc->pad_pclk, timing->clk_rate);
	HDMI_DBG("%s, clk=%d, xres=%d, yres=%d,\n", __func__,
		clk_get_rate(lcdc->pclk), xres, yres);

	hsync_period = (timing->hsync_pulse_width + timing->hsync_back_porch +
			xres + timing->hsync_front_porch);
	hsync_start_x = (timing->hsync_pulse_width + timing->hsync_back_porch);
	hsync_end_x = hsync_period - timing->hsync_front_porch - 1;

	vsync_period = (timing->vsync_pulse_width + timing->vsync_back_porch +
			yres + timing->vsync_front_porch);
	vsync_period *= hsync_period;

	display_vstart = timing->vsync_pulse_width + timing->vsync_back_porch;
	display_vstart *= hsync_period;
	display_vstart += timing->hsync_skew;

	display_vend = timing->vsync_front_porch * hsync_period;
	display_vend = vsync_period - display_vend + timing->hsync_skew - 1;

	/* register values we pre-compute at init time from the timing
	 * information in the panel info */
	lcdc->parms.hsync_ctl = (((hsync_period & 0xfff) << 16) |
				 (timing->hsync_pulse_width & 0xfff));
	lcdc->parms.vsync_period = vsync_period & 0xffffff;
	lcdc->parms.vsync_pulse_width = (timing->vsync_pulse_width *
					 hsync_period) & 0xffffff;

	lcdc->parms.display_hctl = (((hsync_end_x & 0xfff) << 16) |
				    (hsync_start_x & 0xfff));
	lcdc->parms.display_vstart = display_vstart & 0xffffff;
	lcdc->parms.display_vend = display_vend & 0xffffff;
	lcdc->parms.hsync_skew = timing->hsync_skew & 0xfff;
	lcdc->parms.polarity = ((timing->hsync_act_low << 0) |
				(timing->vsync_act_low << 1) |
				(timing->den_act_low << 2));
	lcdc->parms.clk_rate = timing->clk_rate;

	mdp_writel(lcdc->mdp, lcdc->parms.hsync_ctl, MDP_LCDC_HSYNC_CTL);
	mdp_writel(lcdc->mdp, lcdc->parms.vsync_period, MDP_LCDC_VSYNC_PERIOD);
	mdp_writel(lcdc->mdp, lcdc->parms.vsync_pulse_width,
		   MDP_LCDC_VSYNC_PULSE_WIDTH);
	mdp_writel(lcdc->mdp, lcdc->parms.display_hctl, MDP_LCDC_DISPLAY_HCTL);
	mdp_writel(lcdc->mdp, lcdc->parms.display_vstart,
		   MDP_LCDC_DISPLAY_V_START);
	mdp_writel(lcdc->mdp, lcdc->parms.display_vend, MDP_LCDC_DISPLAY_V_END);
	mdp_writel(lcdc->mdp, lcdc->parms.hsync_skew, MDP_LCDC_HSYNC_SKEW);

	mdp_writel(lcdc->mdp, 0, MDP_LCDC_BORDER_CLR);
	mdp_writel(lcdc->mdp, 0x0, MDP_LCDC_UNDERFLOW_CTL);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_HCTL);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_V_START);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_V_END);
	mdp_writel(lcdc->mdp, lcdc->parms.polarity, MDP_LCDC_CTL_POLARITY);
	printk("solomon: polarity=%04x\n", mdp_readl(lcdc->mdp, MDP_LCDC_CTL_POLARITY));

	/* config the dma_p block that drives the lcdc data */
	mdp_writel(lcdc->mdp, lcdc->fb_start, MDP_DMA_P_IBUF_ADDR);
	mdp_writel(lcdc->mdp, (((yres & 0x7ff) << 16) |
			       (xres & 0x7ff)),
		   MDP_DMA_P_SIZE);
	/* TODO: pull in the bpp info from somewhere else? */
	mdp_writel(lcdc->mdp, xres * 2,
		   MDP_DMA_P_IBUF_Y_STRIDE);
	mdp_writel(lcdc->mdp, 0, MDP_DMA_P_OUT_XY);

	dma_cfg = (DMA_PACK_ALIGN_LSB |
		   DMA_PACK_PATTERN_RGB |
		   DMA_DITHER_EN);
	dma_cfg |= DMA_OUT_SEL_LCDC;
	dma_cfg |= DMA_IBUF_FORMAT_RGB565;
	dma_cfg |= DMA_DSTC0G_8BITS | DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;

	mdp_writel(lcdc->mdp, dma_cfg, MDP_DMA_P_CONFIG);
	return 0;
}

static int lcdc_hw_init(struct mdp_lcdc_info *lcdc)
{
	struct msm_panel_data *fb_panel = &lcdc->fb_panel_data;
	uint32_t dma_cfg;
        unsigned int clk_id, clk_rate;

	clk_enable(lcdc->mdp_clk);
	clk_enable(lcdc->pclk);
	clk_enable(lcdc->pad_pclk);

	clk_set_rate(lcdc->pclk, lcdc->parms.clk_rate);
	clk_set_rate(lcdc->pad_pclk, lcdc->parms.clk_rate);
	printk(KERN_DEBUG "pclk = %ld, pad_pclk = %ld\n",
			clk_get_rate(lcdc->pclk),
			clk_get_rate(lcdc->pad_pclk));

	/* write the lcdc params */
	mdp_writel(lcdc->mdp, lcdc->parms.hsync_ctl, MDP_LCDC_HSYNC_CTL);
	mdp_writel(lcdc->mdp, lcdc->parms.vsync_period, MDP_LCDC_VSYNC_PERIOD);
	mdp_writel(lcdc->mdp, lcdc->parms.vsync_pulse_width,
		   MDP_LCDC_VSYNC_PULSE_WIDTH);
	mdp_writel(lcdc->mdp, lcdc->parms.display_hctl, MDP_LCDC_DISPLAY_HCTL);
	mdp_writel(lcdc->mdp, lcdc->parms.display_vstart,
		   MDP_LCDC_DISPLAY_V_START);
	mdp_writel(lcdc->mdp, lcdc->parms.display_vend, MDP_LCDC_DISPLAY_V_END);
	mdp_writel(lcdc->mdp, lcdc->parms.hsync_skew, MDP_LCDC_HSYNC_SKEW);

	mdp_writel(lcdc->mdp, 0, MDP_LCDC_BORDER_CLR);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_UNDERFLOW_CTL);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_HCTL);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_V_START);
	mdp_writel(lcdc->mdp, 0, MDP_LCDC_ACTIVE_V_END);
	mdp_writel(lcdc->mdp, lcdc->parms.polarity, MDP_LCDC_CTL_POLARITY);
	printk("solomon: polarity=%04x\n", mdp_readl(lcdc->mdp, MDP_LCDC_CTL_POLARITY));

	/* config the dma_p block that drives the lcdc data */
	mdp_writel(lcdc->mdp, lcdc->fb_start, MDP_DMA_P_IBUF_ADDR);
	mdp_writel(lcdc->mdp, (((fb_panel->fb_data->yres & 0x7ff) << 16) |
			       (fb_panel->fb_data->xres & 0x7ff)),
		   MDP_DMA_P_SIZE);
	/* TODO: pull in the bpp info from somewhere else? */
	mdp_writel(lcdc->mdp, fb_panel->fb_data->xres * 2,
		   MDP_DMA_P_IBUF_Y_STRIDE);
	mdp_writel(lcdc->mdp, 0, MDP_DMA_P_OUT_XY);

	dma_cfg = (DMA_PACK_ALIGN_LSB |
		   DMA_PACK_PATTERN_RGB |
		   DMA_DITHER_EN);
	dma_cfg |= DMA_OUT_SEL_LCDC;
	dma_cfg |= DMA_IBUF_FORMAT_RGB565;
	dma_cfg |= DMA_DSTC0G_8BITS | DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;

	mdp_writel(lcdc->mdp, dma_cfg, MDP_DMA_P_CONFIG);

	/* Send customized command to ARM9 for escalating DMA_P as tier-1
	 * of AXI bus.
	 * Ref: SR#272509
	 */
	clk_id = P_USB_PHY_CLK;
	clk_rate = 0x1;
	msm_proc_comm(PCOM_CLKCTL_RPC_MIN_RATE, &clk_id, &clk_rate);

	return 0;
}

static void lcdc_wait_vsync(struct msm_panel_data *panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(panel);
	int ret;

	ret = wait_event_timeout(lcdc->vsync_waitq, lcdc->got_vsync, HZ / 2);
	if (ret == 0)
		pr_err("%s: timeout waiting for VSYNC\n", __func__);
	lcdc->got_vsync = 0;
}

static void lcdc_request_vsync(struct msm_panel_data *fb_panel,
			       struct msmfb_callback *vsync_cb)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);

	/* the vsync callback will start the dma */
	vsync_cb->func(vsync_cb);
	lcdc->got_vsync = 0;
	mdp_out_if_req_irq(mdp_dev, MSM_LCDC_INTERFACE, MDP_LCDC_FRAME_START,
			   &lcdc->frame_start_cb);
	lcdc_wait_vsync(fb_panel);
}

static void lcdc_clear_vsync(struct msm_panel_data *fb_panel)
{
	struct mdp_lcdc_info *lcdc = panel_to_lcdc(fb_panel);
	lcdc->got_vsync = 0;
	mdp_out_if_req_irq(mdp_dev, MSM_LCDC_INTERFACE, 0, NULL);
}

/* called in irq context with mdp lock held, when mdp gets the
 * MDP_LCDC_FRAME_START interrupt */
static void lcdc_frame_start(struct msmfb_callback *cb)
{
	struct mdp_lcdc_info *lcdc;

	lcdc = container_of(cb, struct mdp_lcdc_info, frame_start_cb);

	lcdc->got_vsync = 1;
	wake_up(&lcdc->vsync_waitq);
}

static void lcdc_dma_start(void *priv, uint32_t addr, uint32_t stride,
			   uint32_t width, uint32_t height, uint32_t x,
			   uint32_t y)
{
	struct mdp_lcdc_info *lcdc = priv;
	struct mdp_info *mdp = lcdc->mdp;

#if 0
	if (mdp->dma_config_dirty) {
		mdp_writel(lcdc->mdp, 0, MDP_LCDC_EN);
		mdelay(20);
		mdp_dev->configure_dma(mdp_dev);
		mdp_writel(lcdc->mdp, 1, MDP_LCDC_EN);
	}
#endif
	mdp_writel(lcdc->mdp, stride, MDP_DMA_P_IBUF_Y_STRIDE);
	mdp_writel(lcdc->mdp, addr, MDP_DMA_P_IBUF_ADDR);
}

static void precompute_timing_parms(struct mdp_lcdc_info *lcdc)
{
	struct msm_lcdc_timing *timing = lcdc->pdata->timing;
	struct msm_fb_data *fb_data = lcdc->pdata->fb_data;
	unsigned int hsync_period;
	unsigned int hsync_start_x;
	unsigned int hsync_end_x;
	unsigned int vsync_period;
	unsigned int display_vstart;
	unsigned int display_vend;

	hsync_period = (timing->hsync_pulse_width + timing->hsync_back_porch +
			fb_data->xres + timing->hsync_front_porch);
	hsync_start_x = (timing->hsync_pulse_width + timing->hsync_back_porch);
	hsync_end_x = hsync_period - timing->hsync_front_porch - 1;

	vsync_period = (timing->vsync_pulse_width + timing->vsync_back_porch +
			fb_data->yres + timing->vsync_front_porch);
	vsync_period *= hsync_period;

	display_vstart = timing->vsync_pulse_width + timing->vsync_back_porch;
	display_vstart *= hsync_period;
	display_vstart += timing->hsync_skew;

	display_vend = timing->vsync_front_porch * hsync_period;
	display_vend = vsync_period - display_vend + timing->hsync_skew - 1;

	/* register values we pre-compute at init time from the timing
	 * information in the panel info */
	lcdc->parms.hsync_ctl = (((hsync_period & 0xfff) << 16) |
				 (timing->hsync_pulse_width & 0xfff));
	lcdc->parms.vsync_period = vsync_period & 0xffffff;
	lcdc->parms.vsync_pulse_width = (timing->vsync_pulse_width *
					 hsync_period) & 0xffffff;

	lcdc->parms.display_hctl = (((hsync_end_x & 0xfff) << 16) |
				    (hsync_start_x & 0xfff));
	lcdc->parms.display_vstart = display_vstart & 0xffffff;
	lcdc->parms.display_vend = display_vend & 0xffffff;
	lcdc->parms.hsync_skew = timing->hsync_skew & 0xfff;
	lcdc->parms.polarity = ((timing->hsync_act_low << 0) |
				(timing->vsync_act_low << 1) |
				(timing->den_act_low << 2));
	lcdc->parms.clk_rate = timing->clk_rate;
}

static int hdmi_lcdc_probe(struct platform_device *pdev)
{
	struct msm_lcdc_platform_data *pdata = pdev->dev.platform_data;
	struct mdp_lcdc_info *lcdc;
	int ret = 0;

	printk(KERN_DEBUG "%s\n", __func__);

	if (!pdata) {
		pr_err("%s: no LCDC platform data found\n", __func__);
		return -EINVAL;
	}

	_lcdc = lcdc = kzalloc(sizeof(struct mdp_lcdc_info), GFP_KERNEL);
	if (!lcdc)
		return -ENOMEM;

	/* We don't actually own the clocks, the mdp does. */
	lcdc->mdp_clk = clk_get(mdp_dev->dev.parent, "mdp_clk");
	if (IS_ERR(lcdc->mdp_clk)) {
		pr_err("%s: failed to get mdp_clk\n", __func__);
		ret = PTR_ERR(lcdc->mdp_clk);
		goto err_get_mdp_clk;
	}

	lcdc->pclk = clk_get(mdp_dev->dev.parent, "lcdc_pclk_clk");
	if (IS_ERR(lcdc->pclk)) {
		pr_err("%s: failed to get lcdc_pclk\n", __func__);
		ret = PTR_ERR(lcdc->pclk);
		goto err_get_pclk;
	}

	lcdc->pad_pclk = clk_get(mdp_dev->dev.parent, "lcdc_pad_pclk_clk");
	if (IS_ERR(lcdc->pad_pclk)) {
		pr_err("%s: failed to get lcdc_pad_pclk\n", __func__);
		ret = PTR_ERR(lcdc->pad_pclk);
		goto err_get_pad_pclk;
	}

	init_waitqueue_head(&lcdc->vsync_waitq);
	mutex_init(&lcdc->blank_lock);
	lcdc->pdata = pdata;
	lcdc->frame_start_cb.func = lcdc_frame_start;

	platform_set_drvdata(pdev, lcdc);

	mdp_out_if_register(mdp_dev, MSM_LCDC_INTERFACE, lcdc, MDP_DMA_P_DONE,
			    lcdc_dma_start);

	precompute_timing_parms(lcdc);

	lcdc->fb_start = pdata->fb_resource->start;
	lcdc->mdp = container_of(mdp_dev, struct mdp_info, mdp_dev);

	lcdc->fb_panel_data.suspend = lcdc_suspend;
	lcdc->fb_panel_data.resume = lcdc_resume;
	lcdc->fb_panel_data.wait_vsync = lcdc_wait_vsync;
	lcdc->fb_panel_data.request_vsync = lcdc_request_vsync;
	lcdc->fb_panel_data.clear_vsync = lcdc_clear_vsync;
	lcdc->fb_panel_data.blank = lcdc_blank;
	lcdc->fb_panel_data.unblank = lcdc_unblank;
	lcdc->fb_panel_data.adjust_timing = lcdc_adjust_timing;
	lcdc->fb_panel_data.fb_data = pdata->fb_data;
	lcdc->fb_panel_data.interface_type = MSM_LCDC_INTERFACE;

	ret = lcdc_hw_init(lcdc);
	atomic_set(&lcdc->blank_count, 1);
	if (ret) {
		pr_err("%s: Cannot initialize the mdp_lcdc\n", __func__);
		goto err_hw_init;
	}

	lcdc->fb_pdev.name = "msm_hdmi";
	lcdc->fb_pdev.id = pdata->fb_id;
	lcdc->fb_pdev.resource = pdata->fb_resource;
	lcdc->fb_pdev.num_resources = 1;
	lcdc->fb_pdev.dev.platform_data = &lcdc->fb_panel_data;

	ret = platform_device_register(&lcdc->fb_pdev);
	if (ret) {
		pr_err("%s: Cannot register msm_panel pdev\n", __func__);
		goto err_plat_dev_reg;
	}

	pr_info("%s: initialized\n", __func__);

	return 0;

err_plat_dev_reg:
err_hw_init:
	platform_set_drvdata(pdev, NULL);
	clk_put(lcdc->pad_pclk);
err_get_pad_pclk:
	clk_put(lcdc->pclk);
err_get_pclk:
	clk_put(lcdc->mdp_clk);
err_get_mdp_clk:
	kfree(lcdc);
	return ret;
}

static int hdmi_lcdc_remove(struct platform_device *pdev)
{
	struct mdp_lcdc_info *lcdc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	clk_put(lcdc->pclk);
	clk_put(lcdc->pad_pclk);
	kfree(lcdc);

	return 0;
}

static struct platform_driver mdp_lcdc_driver = {
	.probe = hdmi_lcdc_probe,
	.remove = hdmi_lcdc_remove,
	.driver = {
		.name	= "msm_mdp_hdmi",
		.owner	= THIS_MODULE,
	},
};

static int mdp_lcdc_add_mdp_device(struct device *dev,
				   struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (mdp_dev)
		return 0;
	mdp_dev = container_of(dev, struct mdp_device, dev);
	return platform_driver_register(&mdp_lcdc_driver);
}

static void mdp_lcdc_remove_mdp_device(struct device *dev,
				       struct class_interface *class_intf)
{
	/* might need locking if mulitple mdp devices */
	if (dev != &mdp_dev->dev)
		return;
	platform_driver_unregister(&mdp_lcdc_driver);
	mdp_dev = NULL;
}

static struct class_interface mdp_lcdc_interface = {
	.add_dev = &mdp_lcdc_add_mdp_device,
	.remove_dev = &mdp_lcdc_remove_mdp_device,
};

static int __init mdp_lcdc_init(void)
{
	return register_mdp_client(&mdp_lcdc_interface);
}

module_init(mdp_lcdc_init);
