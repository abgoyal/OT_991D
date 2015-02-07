


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/dmi.h>

#define DRV_NAME	"pata_sch"
#define DRV_VERSION	"0.2"

/* see SCH datasheet page 351 */
enum {
	D0TIM	= 0x80,		/* Device 0 Timing Register */
	D1TIM	= 0x84,		/* Device 1 Timing Register */
	PM	= 0x07,		/* PIO Mode Bit Mask */
	MDM	= (0x03 << 8),	/* Multi-word DMA Mode Bit Mask */
	UDM	= (0x07 << 16), /* Ultra DMA Mode Bit Mask */
	PPE	= (1 << 30),	/* Prefetch/Post Enable */
	USD	= (1 << 31),	/* Use Synchronous DMA */
};

static int sch_init_one(struct pci_dev *pdev,
			 const struct pci_device_id *ent);
static void sch_set_piomode(struct ata_port *ap, struct ata_device *adev);
static void sch_set_dmamode(struct ata_port *ap, struct ata_device *adev);

static const struct pci_device_id sch_pci_tbl[] = {
	/* Intel SCH PATA Controller */
	{ PCI_VDEVICE(INTEL, PCI_DEVICE_ID_INTEL_SCH_IDE), 0 },
	{ }	/* terminate list */
};

static struct pci_driver sch_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= sch_pci_tbl,
	.probe			= sch_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
};

static struct scsi_host_template sch_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations sch_pata_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.cable_detect		= ata_cable_unknown,
	.set_piomode		= sch_set_piomode,
	.set_dmamode		= sch_set_dmamode,
};

static struct ata_port_info sch_port_info = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA5,
	.port_ops	= &sch_pata_ops,
};

MODULE_AUTHOR("Alek Du <alek.du@intel.com>");
MODULE_DESCRIPTION("SCSI low-level driver for Intel SCH PATA controllers");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, sch_pci_tbl);
MODULE_VERSION(DRV_VERSION);


static void sch_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	unsigned int pio	= adev->pio_mode - XFER_PIO_0;
	struct pci_dev *dev	= to_pci_dev(ap->host->dev);
	unsigned int port	= adev->devno ? D1TIM : D0TIM;
	unsigned int data;

	pci_read_config_dword(dev, port, &data);
	/* see SCH datasheet page 351 */
	/* set PIO mode */
	data &= ~(PM | PPE);
	data |= pio;
	/* enable PPE for block device */
	if (adev->class == ATA_DEV_ATA)
		data |= PPE;
	pci_write_config_dword(dev, port, data);
}


static void sch_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	unsigned int dma_mode	= adev->dma_mode;
	struct pci_dev *dev	= to_pci_dev(ap->host->dev);
	unsigned int port	= adev->devno ? D1TIM : D0TIM;
	unsigned int data;

	pci_read_config_dword(dev, port, &data);
	/* see SCH datasheet page 351 */
	if (dma_mode >= XFER_UDMA_0) {
		/* enable Synchronous DMA mode */
		data |= USD;
		data &= ~UDM;
		data |= (dma_mode - XFER_UDMA_0) << 16;
	} else { /* must be MWDMA mode, since we masked SWDMA already */
		data &= ~(USD | MDM);
		data |= (dma_mode - XFER_MW_DMA_0) << 8;
	}
	pci_write_config_dword(dev, port, data);
}


static int __devinit sch_init_one(struct pci_dev *pdev,
				   const struct pci_device_id *ent)
{
	static int printed_version;
	const struct ata_port_info *ppi[] = { &sch_port_info, NULL };

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev,
			   "version " DRV_VERSION "\n");

	return ata_pci_bmdma_init_one(pdev, ppi, &sch_sht, NULL, 0);
}

static int __init sch_init(void)
{
	return pci_register_driver(&sch_pci_driver);
}

static void __exit sch_exit(void)
{
	pci_unregister_driver(&sch_pci_driver);
}

module_init(sch_init);
module_exit(sch_exit);