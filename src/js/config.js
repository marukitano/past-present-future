module.exports = [
  {
    type: "heading",
    defaultValue: "Past Present Future"
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Animation"
      },
      {
        type: "select",
        messageKey: "AnimationEffect",
        label: "Effect",
        defaultValue: "2",
        options: [
          {
            label: "None",
            value: "0"
          },
          {
            label: "Bounce",
            value: "1"
          },
          {
            label: "Pixel Wave",
            value: "2"
          }
        ]
      },
      {
        type: "select",
        messageKey: "AnimationSpeed",
        label: "Speed",
        defaultValue: "1",
        options: [
          {
            label: "Slow",
            value: "0"
          },
          {
            label: "Normal",
            value: "1"
          },
          {
            label: "Fast",
            value: "2"
          }
        ]
      }
    ]
  },
  {
    type: "section",
    items: [
      {
        type: "heading",
        defaultValue: "Display"
      },
      {
        type: "toggle",
        messageKey: "ShowDate",
        label: "Date",
        defaultValue: true
      },
      {
        type: "toggle",
        messageKey: "ShowTemperature",
        label: "Temperature",
        defaultValue: true
      },
      {
        type: "toggle",
        messageKey: "ShowSwissEmblem",
        label: "Swiss emblem",
        defaultValue: true
      }
    ]
  },
  {
    type: "submit",
    defaultValue: "Save"
  }
];
