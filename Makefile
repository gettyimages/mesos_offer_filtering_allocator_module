.PHONY: build run rm dev dev-restart release docker-build-images
run:
	@scripts/run.sh $(filter-out $@,$(MAKECMDGOALS))

build:
	@scripts/build.sh $(filter-out $@,$(MAKECMDGOALS))

release:
	@scripts/build.sh release $(filter-out $@,$(MAKECMDGOALS))

rm:
	@scripts/run.sh rm

dev-restart:
	@scripts/run.sh rm && scripts/run.sh start && scripts/run.sh logs master-one master-two master-three

dev:
	@scripts/build.sh && scripts/run.sh rm && scripts/run.sh start && scripts/run.sh logs master-one master-two master-three

%:
	@:
