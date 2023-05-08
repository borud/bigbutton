# bigbutton

I'm not sure this is useful for anyone but me, but the code is shared here for reference.

## config

Copy (or rename) the `src/config.h-template` file to `src/config.h` and configure the
settings.

## build

To build and flash the image:

```shell
pio run -t upload
```

You can start the monitor as well after uploading.

```shell
pio run -t upload -t monitor
```
