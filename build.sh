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

	CPPC=${arch}-w64-mingw32-g++
	CC=${arch}-w64-mingw32-gcc
	$CC -g -fPIC -c logging.c -o $OUT_DIR/logging.o
	$CPPC -g -fPIC -c common.cpp -std=c++20 -o $OUT_DIR/common.o
	$CPPC -g -fPIC -c config.cpp -Ijson_hpp -std=c++20 -o $OUT_DIR/config.o
	$CPPC -g -fPIC -c dinput8_hook_adapter.cpp -std=c++20 -o $OUT_DIR/dinput8_hook_adapter.o
	$CPPC -g -fPIC -c tdu2_physics_tweaks.cpp -Iminhook_1.3.3/include -std=c++20 -o $OUT_DIR/tdu2_physics_tweaks.o -O0
	$CPPC -g -shared -o $OUT_DIR/tdu2_physics_tweaks_${arch}.asi $OUT_DIR/logging.o $OUT_DIR/common.o $OUT_DIR/config.o $OUT_DIR/dinput8_hook_adapter.o $OUT_DIR/tdu2_physics_tweaks.o -Lminhook_1.3.3/bin -lntdll -lkernel32 -ldxguid -Wl,-Bstatic -lpthread -l${min_hook_lib} -static-libgcc -static-libstdc++

	rm $OUT_DIR/*.o

	cp tdu2_physics_tweaks_config.json $OUT_DIR/
	cp ultimate_asi_loader/dinput8.dll $OUT_DIR/
	cp dinput8_ffb_tweaks_prebuilt/dinput8_ffb_tweaks_i686.dll $OUT_DIR/
done

