include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/ft5206

obj-y += mtk_gslX68X.o gsl_point_id.o
$(obj)/gsl_point_id.o: $(srctree)/$(obj)/gsl_point_id
		cp $(srctree)/$(obj)/gsl_point_id  $(obj)/gsl_point_id.o

#obj-y	+=  focaltech_ex_fun.o

