DATE = $(shell date +%Y%m%d)

SUBDIRS = \
	addons \
	src

DIST_SUBDIRS = \
	$(SUBDIRS)

install: all
	mkdir -m 755 -p $(DESTDIR)@LIBDIR@/pvr.p2pproxy
	mkdir -m 755 -p $(DESTDIR)@DATADIR@/pvr.p2pproxy
	cp -r addons/pvr.p2pproxy .
	$(STRIP) -x pvr.p2pproxy/$(ADDONNAME)
	cp -f pvr.p2pproxy/$(ADDONNAME) $(DESTDIR)@LIBDIR@/pvr.p2pproxy ; chmod 655 $(DESTDIR)@LIBDIR@/pvr.p2pproxy/$(ADDONNAME)
	cp -r -f @abs_top_srcdir@/addons/pvr.p2pproxy/* $(DESTDIR)@DATADIR@/pvr.p2pproxy ; chmod -R o+rx $(DESTDIR)@DATADIR@/pvr.p2pproxy
	rm -Rf pvr.p2pproxy

uninstall:
	rm -Rf $(DESTDIR)@LIBDIR@/pvr.p2pproxy
	rm -Rf $(DESTDIR)@DATADIR@/pvr.p2pproxy

dist-zip: all
	cp -r addons/pvr.p2pproxy .
	$(STRIP) -x pvr.p2pproxy/$(ADDONNAME)
	zip -9 -r ./pvr.p2pproxy.$(VERSION).zip pvr.p2pproxy --exclude \*.am \*.in \*Makefile \*.gitignore
	rm -Rf pvr.p2pproxy

dist-zip-daily: all
	cp -r addons/pvr.p2pproxy .
	cat pvr.p2pproxy/addon.xml | sed "s/$(VERSION)/$(VERSION).$(DATE)/g" > pvr.p2pproxy/addon.xml.new
	mv pvr.p2pproxy/addon.xml.new pvr.p2pproxy/addon.xml
if !APPLE
	$(STRIP) -s -x pvr.p2pproxy/$(ADDONNAME)
endif
if MINGW32
	zip -9 -r ./pvr.p2pproxy.$(VERSION).$(DATE).zip pvr.p2pproxy --exclude \*.am \*.in \*Makefile \*.gitignore
else
	zip -9 -r ./pvr.p2pproxy.$(VERSION).$(DATE).zip pvr.p2pproxy --exclude \*.am \*.in \*Makefile \*.gitignore \*.dll
endif
	rm -Rf pvr.p2pproxy

dist-git: all
	GIT_REV=`git --no-pager log --abbrev=7 -n 1 --pretty=format:"%h %ci" HEAD | cut -f1 -d' '` ; \
	make VERSION=$$GIT_REV dist

repo: all dist-zip
	make -C data all
	rm -Rf addon-repository
	mkdir -p addon-repository/pvr.p2pproxy
	cp pvr.p2pproxy.$(VERSION).zip addon-repository/pvr.p2pproxy
	cp addons/pvr.p2pproxy/icon.png addon-repository/pvr.p2pproxy
	cp data/addons.xml addon-repository
	cp data/addons.xml.md5 addon-repository

repo-daily: all dist-zip-daily
	make -C data all
	rm -Rf addon-repository
	mkdir -p addon-repository/pvr.p2pproxy
	cp pvr.p2pproxy.$(VERSION).$(DATE).zip addon-repository/pvr.p2pproxy
	cp addons/pvr.p2pproxy/icon.png addon-repository/pvr.p2pproxy
	cat data/addons.xml | sed "s/$(VERSION)/$(VERSION).$(DATE)/g" > addon-repository/addons.xml
	cd addon-repository && md5sum addons.xml > addons.xml.md5

release-repo-daily: repo-daily
	cp -r addon-repository/* ~/Dropbox/Public/xbmc-addon-xvdr

repo-zip: all
	cd addons && \
	zip -9 -r ../repository.p2pproxy.linux32.zip repository.p2pproxy.linux32 --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.p2pproxy.linux64.zip repository.p2pproxy.linux64 --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.p2pproxy.rpi.zip repository.p2pproxy.rpi --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.p2pproxy.osx.zip repository.p2pproxy.osx --exclude \*.am \*.in \*Makefile \*.gitignore && \
	zip -9 -r ../repository.p2pproxy.win32.zip repository.p2pproxy.win32 --exclude \*.am \*.in \*Makefile \*.gitignore
