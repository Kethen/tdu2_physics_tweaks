set -xe

#for arch in x86_64 i686
for arch in i686
do
	OUT_DIR=dist/${arch}
	rm -rf $OUT_DIR
	mkdir -p $OUT_DIR

	min_hook_lib="MinHook.x86"
	if [ ${arch} == x86_64 ]
	then
		min_hook_lib="MinHook.x64"
	fi
	cp minhook_1.3.3/bin/${min_hook_lib}.dll $OUT_DIR

	CSTD="-std=c11"
	CPPSTD="-std=c++11"
	CPPC="${arch}-w64-mingw32-clang++ $CPPSTD -stdlib=libc++"
	CC="${arch}-w64-mingw32-clang $CSTD"
	LINKER="${arch}-w64-mingw32-clang++ -stdlib=libc++"

	$CC -g -fPIC -c logging.c -o $OUT_DIR/logging.o -O2
	$CPPC -g -fPIC -c common.cpp -o $OUT_DIR/common.o -O2
	$CPPC -g -fPIC -c config.cpp -Ijson_hpp -o $OUT_DIR/config.o -O2
	$CPPC -g -fPIC -c dinput8_hook_adapter.cpp -o $OUT_DIR/dinput8_hook_adapter.o -O2
	$CPPC -g -fPIC -c process.cpp -o $OUT_DIR/process.o -O2
	$CPPC -g -fPIC -c tdu2_physics_tweaks.cpp -Iminhook_1.3.3/include -o $OUT_DIR/tdu2_physics_tweaks.o -O0
	$LINKER -shared $OUT_DIR/logging.o $OUT_DIR/common.o $OUT_DIR/config.o $OUT_DIR/dinput8_hook_adapter.o $OUT_DIR/process.o $OUT_DIR/tdu2_physics_tweaks.o -Lminhook_1.3.3/bin -lntdll -lkernel32 -ldxguid -Wl,-Bstatic -lpthread -Wl,-Bstatic -lc++ -l${min_hook_lib} -o $OUT_DIR/tdu2_physics_tweaks_${arch}.asi

	rm $OUT_DIR/*.o

	cp tdu2_physics_tweaks_config.json $OUT_DIR/
	cp ultimate_asi_loader/dinput8.dll $OUT_DIR/
	cp dinput8_ffb_tweaks_prebuilt/dinput8_ffb_tweaks_i686.dll $OUT_DIR/
done

