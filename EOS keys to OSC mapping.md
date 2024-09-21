# EOS DATA MAPPING

# key: autohotkey rules: ^ is ctrl, + is shift, ! is alt

# <https://www.autohotkey.com/docs/v1/Hotkeys.htm>

first column is the key on the keyboard via nomad software or console software.
 second column is the key's OSC command to send to the EOS console
 preceed every command with /eos/key/ and then the word. Key up and key down events need to be processed separately.
 ctrl and alt are currently missing.
 note that the shift modifier is always sent to the console.

|   | norm         | ctrl                   | alt               | ctrl + alt      | shift (but irrelevant) |
|---|--------------|------------------------|-------------------|-----------------|------------------------|
| a | at           | select_active          | address           | playbackassert  |                        |
| b | block        | beam                   | beam_palette      | intensity_block |                        |
| c | copy_to      | color                  | color_palette     | scroller_frame  |                        |
| d | delay        | data                   |                   | follow          |                        |
| e | recall_from  |                        | effect            |                 |                        |
| f | full         | focus                  |                   | freeze          |                        |
| g | group        | go_to_cue              | spacebar_go       | go_to_cue_0     |                        |
| h | rem_dim      | home                   |                   | highlight       |                        |
| i | time         | intensity              | intensity_palette | display_timing  |                        |
| j | trace        |                        |                   |                 |                        |
| k | mark         | popup_virtual_keyboard | park              |                 |                        |
| l | label        | select_last            | learn             |                 |                        |
| m | macro        | select_manual          | magic_sheet       |                 |                        |
| n | sneak        | allnps                 |                   |                 |                        |
| o | out          | offset                 |                   |                 |                        |
| p | part         |                        | preset            | capture         |                        |
| q | cue          | query                  | stopback          |                 |                        |
| r | record       | record_only            |                   |                 |                        |
| s | sub          | snapshot               | setup             |                 |                        |
| t | thru         |                        |                   | timing_disable  |                        |
| u | update       |                        |                   | focus_wand      |                        |
| v | fader_pages  | level                  |                   | notes           |                        |
| w | fan_         | assert                 |                   | color_path      |                        |
| x | cueonlytrack | undo                   | pixelmap          |                 |                        |
| y | about        |                        |                   |                 |                        |
| z | shift        |                        |                   |                 |                        |

1
2
3
4
5
6
7
8
9
0
-

=
[
]
\       highlight
;       patch
'
,       ,
.       .
/       \

esc
f1
f2
f3
f4
f5
f6
f7
f8
f9
f10
f11
f12
del
pgup
pgdn
backspace
enter
ctrl
alt
shift       shift

^space: /eos/key/Stop_Back_Main_CueList

f6::/eos/key/staging_mode
