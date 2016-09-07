.PHONY: build test dev release
test:
	@scripts/test.sh

build:
	@scripts/build.sh

dev:
	@scripts/build.sh && scripts/test.sh rm && scripts/test.sh start && scripts/test.sh logs master-one master-two master-three

release:
	@scripts/build.sh release

