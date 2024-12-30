format_code:
	python Script/formatCode.py

golang:
	docker pull --platform=$(PLATFORM) gcc:11.5.0
	docker buildx build --platform=$(PLATFORM) --target export-stage --output type=local,dest=./output/$(PLATFORM) . 