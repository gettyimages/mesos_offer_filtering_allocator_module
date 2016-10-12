.PHONY: build run rm dev dev-restart release docker-build-images
build:
	@scripts/build.sh $(filter-out $@,$(MAKECMDGOALS))

release:
	@scripts/build.sh release $(filter-out $@,$(MAKECMDGOALS))

run:
	@DOCKER_USER="" scripts/run.sh $(filter-out $@,$(MAKECMDGOALS))

rm:
	@DOCKER_USER="" scripts/run.sh rm

dev-restart:
	@DOCKER_USER="" scripts/run.sh rm && scripts/run.sh start && scripts/run.sh logs master-one master-two master-three

dev:
	@DOCKER_USER="" scripts/build.sh && scripts/run.sh rm && scripts/run.sh start && scripts/run.sh logs master-one master-two master-three

%:
	@:
