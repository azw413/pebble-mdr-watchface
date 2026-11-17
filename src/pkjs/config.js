// Settings page, rendered on the phone by Clay (@rebble/clay).
// Colour swatches are each theme's bright (time) colour; keep them in sync with
// the RAMPS table in src/c/theme.c.
module.exports = [
  {
    type: 'heading',
    defaultValue: 'MDR'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Display'
      },
      {
        type: 'radiogroup',
        messageKey: 'Layout',
        label: 'Style',
        defaultValue: '0',
        options: [
          { label: 'Standard: time and date in a band, with bins', value: '0' },
          { label: 'Large: stacked hours and minutes', value: '1' }
        ]
      },
      {
        type: 'color',
        messageKey: 'Colour',
        label: 'Colour',
        description: 'The time uses this colour. The date, outlines and number field use darker shades of it.',
        defaultValue: 'AAFFFF',
        sunlight: false,
        layout: [
          ['AAFFFF', 'AAFFAA', 'FFAA55', 'FFAAAA'],
          ['FFAAFF', 'AAAAFF', 'FFFFAA', 'FFFFFF']
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Refinement'
      },
      {
        type: 'select',
        messageKey: 'Interval',
        label: 'Animate every',
        description: 'How often the old time is refined into the new one. Each run lasts about 5 seconds, so short intervals use more battery.',
        defaultValue: '5',
        options: [
          { label: '1 minute', value: '1' },
          { label: '5 minutes', value: '5' },
          { label: '10 minutes', value: '10' },
          { label: '15 minutes', value: '15' },
          { label: '30 minutes', value: '30' },
          { label: '1 hour', value: '60' },
          { label: 'Never', value: '0' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
