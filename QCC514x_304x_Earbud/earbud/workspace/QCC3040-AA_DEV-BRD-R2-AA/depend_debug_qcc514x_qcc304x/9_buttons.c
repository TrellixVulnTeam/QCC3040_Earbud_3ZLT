#include "input_event_manager.h"
#include "9_buttons.h"

const InputEventConfig_t input_event_config = 
{
	/* Table to convert from PIO to input event ID*/
	{
		 0, -1,  1,  2, -1, -1, -1, -1, -1,  3,  4,  5,  6,  7,  8, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	},

	/* Masks for each PIO bank to configure as inputs */
	{ 0x00007e0dUL, 0x00000000UL, 0x00000000UL },
	/* PIO debounce settings */
	4, 5
};


const InputActionMessage_t default_message_group[39] = 
{
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_8SEC,             /* Message */
	},
	{
		MUSIC,                                  /* Input event bits */
		MUSIC,                                  /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_PLAY_PAUSE_TOGGLE,           /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_FORWARD_HELD,                /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_VA_BUTTON_RELEASE,                  /* Message */
	},
	{
		VOL_MINUS,                              /* Input event bits */
		VOL_MINUS,                              /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_LEAKTHROUGH_DISABLE,                /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_VA_BUTTON_HELD_1SEC,                /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_BACKWARD_HELD_RELEASE,       /* Message */
	},
	{
		VOL_PLUS,                               /* Input event bits */
		VOL_PLUS,                               /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_VOLUME_UP_RELEASE,           /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_3SEC,                /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_6SEC,                /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		12000,                                  /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_DFU,                 /* Message */
	},
	{
		VOL_PLUS,                               /* Input event bits */
		VOL_PLUS | VOL_MINUS,                   /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_VOLUME_UP,                   /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		15000,                                  /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_FACTORY_RESET_DS,    /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_BACKWARD_HELD,               /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_VA_BUTTON_DOWN,                     /* Message */
	},
	{
		VOL_PLUS,                               /* Input event bits */
		VOL_PLUS,                               /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_LEAKTHROUGH_ENABLE,                 /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_1SEC,                /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_FORWARD_HELD_RELEASE,        /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		1,                                      /* Count */
		APP_BUTTON_BACKWARD,                    /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		1,                                      /* Count */
		LI_MFB_BUTTON_SINGLE_PRESS,             /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		5,                                      /* Count */
		LI_MFB_MFB_BUTTON_5_CLICKS,             /* Message */
	},
	{
		VOICE,                                  /* Input event bits */
		VOICE,                                  /* Input event mask */
		MULTI_CLICK,                            /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		2,                                      /* Count */
		APP_ANC_ENABLE,                         /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		12000,                                  /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_DFU,              /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		15000,                                  /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_FACTORY_RESET_DS, /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_6SEC,             /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_3SEC,             /* Message */
	},
	{
		VOL_MINUS,                              /* Input event bits */
		VOL_MINUS | VOL_PLUS,                   /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_VOLUME_DOWN,                 /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_RELEASE_1SEC,             /* Message */
	},
	{
		VOICE,                                  /* Input event bits */
		VOICE,                                  /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_ANC_SET_NEXT_MODE,                  /* Message */
	},
	{
		VOICE,                                  /* Input event bits */
		VOICE,                                  /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		1,                                      /* Count */
		APP_ANC_TOGGLE_ON_OFF,                  /* Message */
	},
	{
		VOICE,                                  /* Input event bits */
		VOICE,                                  /* Input event mask */
		HELD_RELEASE,                           /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_ANC_DISABLE,                        /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_VA_BUTTON_HELD_RELEASE,             /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_LEAKTHROUGH_SET_NEXT_MODE,          /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		LI_MFB_BUTTON_HELD_8SEC,                /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		1,                                      /* Count */
		APP_VA_BUTTON_SINGLE_CLICK,             /* Message */
	},
	{
		VOL_MINUS,                              /* Input event bits */
		VOL_MINUS,                              /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_BUTTON_VOLUME_DOWN_RELEASE,         /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		2,                                      /* Count */
		APP_VA_BUTTON_DOUBLE_CLICK,             /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		2000,                                   /* Timeout */
		0,                                      /* Repeat */
		0,                                      /* Count */
		APP_LEAKTHROUGH_TOGGLE_ON_OFF,          /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		MULTI_CLICK,                            /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		1,                                      /* Count */
		APP_BUTTON_FORWARD,                     /* Message */
	},
};


ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(LOGICAL_INPUT,MAX_INPUT_ACTION_MESSAGE_ID)

