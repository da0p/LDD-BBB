
SUBDIRS = pseudo-chardev pseudo-platform platform-dev-with-dt \
	platform-dev-with-dt-overlay pcd_sysfs gpio_sysfs

all: subdirs

subdirs:
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done

clean:
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done
