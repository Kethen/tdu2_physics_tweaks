IMAGE_NAME=docker.io/mstorsjo/llvm-mingw:latest

podman run \
	--rm -it \
	--security-opt label=disable \
	-v ./:/work_dir \
	-w /work_dir \
	--entrypoint /bin/bash \
	$IMAGE_NAME \
	build.sh
