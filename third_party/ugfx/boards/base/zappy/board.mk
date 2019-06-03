GFXINC  +=	$(GFXLIB)/boards/base/zappy

GFXSRC  +=

ifeq ($(OPT_OS),raw32)
	GFXDEFS +=	
	GFXSRC	+=
	GFXSRC	+=
	GFXDEFS	+=	GFX_OS_PRE_INIT_FUNCTION=Raw32OSInit GFX_OS_INIT_NO_WARNING=GFXON
	GFXINC	+=
	LDSCRIPT =
endif

include $(GFXLIB)/drivers/gdisp/SSD1322/driver.mk
