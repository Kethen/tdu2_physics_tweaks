IMAGE_NAME=tdu2_physics_tweaks

if ! podman image exists $IMAGE_NAME
then
	podman image build -t $IMAGE_NAME -f DockerFile
fi

podman run \
	--rm -it \
	--security-opt label=disable \
	-v ./:/work_dir \
	-w /work_dir \
	--entrypoint /bin/bash \
	$IMAGE_NAME \
	build.sh
