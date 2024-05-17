# Stardust-tests
Contains tests and functionality for playing full games against opponents.

The following bots are redistributed as test opponents, as they have permissive licenses:

- McRave: https://github.com/Cmccrave/McRave
- Steamhammer: https://satirist.org/ai/starcraft/steamhammer
- Iron: https://bwem.sourceforge.net/Iron.html
- Locutus: https://github.com/bmnielsen/Locutus

They have been modified to compile in clang and to add some hooks useful for testing (like allowing the test to specify the strategy for the opponent to use).

Some opponents are referenced that are not redistributed, as they do not have permissive licenses. If you are forking this repository, you will therefore either have to remove the references to these opponents to allow the project to compile, or find sources for these opponents and integrate them yourself.
