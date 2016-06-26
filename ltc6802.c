#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

enum ltc6802_id {
	ltc6802
};

static const struct spi_device_id ltc6802_id[] = {
	{"ltc6802", ltc6802},
	{}
};
MODULE_DEVICE_TABLE(spi, ltc6802_id);

#ifdef CONFIG_OF
static const struct of_device_id ltc6802_adc_dt_ids[] = {
	{ .compatible = "linear,ltc6802" },
	{},
};
MODULE_DEVICE_TABLE(of, ltc6802_adc_dt_ids);
#endif

#define LTC6802_V_CHAN(index)						\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = index,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE),	\
		.scan_index = index + 1,				\
		.scan_type = {						\
			.sign = 'u',					\
			.realbits = 12,					\
			.storagebits = 16,				\
			.endianness = IIO_BE,				\
		},							\
	}

#define LTC6802_T_CHAN(index)						\
	{								\
		.type = IIO_TEMP,					\
		.indexed = 1,						\
		.channel = index,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE),	\
		.scan_index = index + 1,				\
		.scan_type = {						\
			.sign = 'u',					\
			.realbits = 12,					\
			.storagebits = 16,				\
			.endianness = IIO_BE,				\
		},							\
	}

static const struct iio_chan_spec ltc6802_channels[] = {
	LTC6802_T_CHAN(0),
	LTC6802_T_CHAN(1),
	LTC6802_T_CHAN(2),
	LTC6802_V_CHAN(0),
	LTC6802_V_CHAN(1),
	LTC6802_V_CHAN(2),
	LTC6802_V_CHAN(3),
	LTC6802_V_CHAN(4),
	LTC6802_V_CHAN(5),
	LTC6802_V_CHAN(6),
	LTC6802_V_CHAN(7),
	LTC6802_V_CHAN(8),
	LTC6802_V_CHAN(9),
	LTC6802_V_CHAN(10),
	LTC6802_V_CHAN(11),
	LTC6802_V_CHAN(12),
};

struct ltc6802_chip_info {
	const struct iio_chan_spec *channels;
	unsigned int num_channels;
};

static const struct ltc6802_chip_info ltc6802_chip_info_tbl[] = {
	[ltc6802] = {
		.channels = ltc6802_channels,
		.num_channels = ARRAY_SIZE(ltc6802_channels),
	},
};

struct ltc6802_state {
	const struct ltc6802_chip_info	*info;
	struct spi_device		*spi;
	__be16				*buffer;
	struct mutex			lock;
	u8				reg ____cacheline_aligned;
};

static int ltc6802_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	int ret = 0;
	struct ltc6802_state *st = iio_priv(indio_dev);

	mutex_lock(&st->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		//ret = ltc6802_read_single_value(indio_dev, chan, val);
		ret = IIO_VAL_INT;
		*val = 33000;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_TEMP:
		case IIO_VOLTAGE:
			*val = 3065 * 2;
			*val2 = 12;
			ret = IIO_VAL_FRACTIONAL_LOG2;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&st->lock);

	return ret;
}

static const struct iio_info ltc6802_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &ltc6802_read_raw,
};

static int ltc6802_probe(struct spi_device *spi)
{
	int ret;
	struct iio_dev *indio_dev;
	struct ltc6802_state *st;

	pr_info("%s: probe(spi = 0x%p)\n", __func__, spi);

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("Can't allocate iio device\n");
		return -ENOMEM;
	}

	spi_set_drvdata(spi, indio_dev);

	st = iio_priv(indio_dev);
	st->spi = spi;
	st->info = &ltc6802_chip_info_tbl[spi_get_device_id(spi)->driver_data];

	mutex_init(&st->lock);

	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->dev.parent = &spi->dev;
	indio_dev->info = &ltc6802_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = st->info->channels;
	indio_dev->num_channels = st->info->num_channels;

	st->buffer = devm_kmalloc(&indio_dev->dev,
				  indio_dev->num_channels * 2,
				  GFP_KERNEL);
	if (st->buffer == NULL) {
		dev_err(&indio_dev->dev, "Can't allocate buffer\n");
		return -ENOMEM;
	}

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&indio_dev->dev, "Failed to register iio device\n");
	}

	return ret;
}

static int ltc6802_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	pr_info("%s: remove(spi = 0x%p)\n", __func__, spi);

	iio_device_unregister(indio_dev);

	return 0;
}

static struct spi_driver ltc6802_driver = {
	.driver = {
		.name	= "ltc6802",
		.of_match_table = of_match_ptr(ltc6802_adc_dt_ids),
	},
	.probe		= ltc6802_probe,
	.remove		= ltc6802_remove,
	.id_table	= ltc6802_id,
};
module_spi_driver(ltc6802_driver);

MODULE_AUTHOR("Olivier C. Larocque <olivier.c.larocque@gmail.com>");
MODULE_DESCRIPTION("LTC6802 Battery Stack Monitor");
MODULE_LICENSE("GPL v2");