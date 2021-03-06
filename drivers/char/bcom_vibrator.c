#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/timed_gpio.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif /*CONFIG_HAS_WAKELOCK*/


#include "../staging/android/timed_output.h"

#define VIB_ON 1
#define VIB_OFF 0

#if defined(CONFIG_HAS_WAKELOCK)
	struct wake_lock vib_wl;
#endif /*CONFIG_HAS_WAKELOCK*/

static struct timed_output_dev vibrator_timed_dev;
static struct timer_list	vibrate_timer;
static struct work_struct	vibrator_off_work;
static struct regulator *vib_regulator=NULL;
static int vib_voltage;
static int Is_vib_shortly;


static void vibrator_ctrl_regulator(int on_off)
{
	printk(KERN_NOTICE "Vibrator: %s\n",(on_off?"ON":"OFF"));
	
	if(on_off==VIB_ON)
	{
		if(!regulator_is_enabled(vib_regulator))
		{
			regulator_set_voltage(vib_regulator,vib_voltage,vib_voltage);
			regulator_enable(vib_regulator);
			printk(KERN_NOTICE "Vibrator: enable\n");
		}
	}
	else
	{
		if(regulator_is_enabled(vib_regulator))
		{
			regulator_disable(vib_regulator);
			printk(KERN_NOTICE "Vibrator: disable\n");
		}
	}
}
static void vibrator_off_worker(struct work_struct *work)
{
	vibrator_ctrl_regulator(VIB_OFF);

#if defined(CONFIG_HAS_WAKELOCK)
	wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
}

static void on_vibrate_timer_expired(unsigned long x)
{
	Is_vib_shortly = false;
	schedule_work(&vibrator_off_work);	
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
	int timeout)
{
	int ret_mod_timer = 0;

	printk(KERN_NOTICE "Vibrator: Set duration: %dms\n", timeout);

	if( timeout==0 )
	{
		if(Is_vib_shortly == false){
			vibrator_ctrl_regulator(VIB_OFF);
			del_timer(&vibrate_timer);
		}
		return;
	}

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/

	vibrator_ctrl_regulator(VIB_ON);
	if(timeout < 100){
		Is_vib_shortly = true;
		timeout *= 2;
	}
	if(timeout == 5000){
		printk(KERN_NOTICE "Vibrator: timeout= %dms\n", timeout);
        }
	else{
		if(!timer_pending(&vibrate_timer) ){
			ret_mod_timer = mod_timer(&vibrate_timer, jiffies + msecs_to_jiffies(timeout));
	                if( ret_mod_timer ){
				printk(KERN_NOTICE "Vibrator: ret_mod_timer= %d\n", ret_mod_timer);
                                vibrator_ctrl_regulator(VIB_OFF);
                        }
                }
		else
		{
#if defined(CONFIG_HAS_WAKELOCK)
			wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
		}
	}		
	return;

}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	int retTime = jiffies_to_msecs(jiffies-vibrate_timer.expires);
	printk(KERN_NOTICE "Vibrator: Current duration: %dms\n", retTime);
	return retTime;
}

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;
	/* Setup timed_output obj */
	vibrator_timed_dev.name = "vibrator";
	vibrator_timed_dev.enable = vibrator_enable_set_timeout;
	vibrator_timed_dev.get_time = vibrator_get_remaining_time;
	vib_voltage = pdev->voltage;

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_init(&vib_wl, WAKE_LOCK_SUSPEND, __stringify(vib_wl));
#endif

	/* Vibrator dev register in /sys/class/timed_output/ */
	ret = timed_output_dev_register(&vibrator_timed_dev);
	if (ret < 0) {
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		timed_output_dev_unregister(&vibrator_timed_dev);
	}

	init_timer(&vibrate_timer);
	vibrate_timer.function = on_vibrate_timer_expired;
	vibrate_timer.data = (unsigned long)NULL;
	INIT_WORK(&vibrator_off_work,
		 vibrator_off_worker);
	Is_vib_shortly = false;
	return 0;
}

static int __devexit vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&vibrator_timed_dev);
	return 0;
}

static int vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vibrator_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe		= vibrator_probe,
	.remove		= __devexit_p(vibrator_remove),
	.suspend		= vibrator_suspend,
	.resume		=  vibrator_resume,
	.driver		= {
		.name	= "vibrator",
		.owner	= THIS_MODULE,
	},
};

static int __init vibrator_init(void)
{

	vib_regulator = regulator_get(NULL,"vib_vcc");
	
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	if (vib_regulator) 
	{
       	 regulator_put(vib_regulator);
		 vib_regulator = NULL;
    	}
	platform_driver_unregister(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Android Vibrator driver");
MODULE_LICENSE("GPL");
