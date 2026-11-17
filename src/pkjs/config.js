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
    type: 'submit',
    defaultValue: 'Save'
  }
];
