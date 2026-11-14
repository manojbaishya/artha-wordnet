Feature: Artha thesaurus application behaviour
  As a user of Artha
  I want the application to start, survive, and respond to lookups
  So that I can find synonyms and definitions

  Behavioural (app + UI) layer of the test pyramid.
  Scenarios here drive the real artha binary end-to-end on a scratch HOME
  and an X display (Xephyr :94 per project rules; :0 is read-only).

  Background:
    Given a scratch HOME and XDG_* directory
    And the artha binary is built and staged

  Scenario: Application survives startup and writes preferences
    When I launch artha on the display
    Then artha stays running for at least 2 seconds
    And artha writes its preferences file into the scratch config dir
    When I stop artha
    Then the scratch dir contains only artha-owned files

  Scenario: Binary launches without a display is handled gracefully
    Given no DISPLAY is set
    When I launch artha
    Then artha exits (does not hang indefinitely)
