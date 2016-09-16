.PHONY: build run dev release
run:
	@scripts/test.sh $(filter-out $@,$(MAKECMDGOALS))

build:
	@scripts/build.sh $(filter-out $@,$(MAKECMDGOALS))

release:
	@scripts/build.sh release

dev-restart:
	@scripts/test.sh rm && scripts/test.sh start && scripts/test.sh logs master-one master-two master-three

dev:
	@scripts/build.sh && scripts/test.sh rm && scripts/test.sh start && scripts/test.sh logs master-one master-two master-three

%:
	@:
