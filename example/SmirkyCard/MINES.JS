// A sibling script; the game will follow in a later change.
card('first', class {
  compose() {
    return VStack(
      Text('Minesweeper (coming soon)').TEST_ID('SmirkyCard.Title'),
      Button('./MAIN.JS', () => open('./MAIN.JS')).TEST_ID('SmirkyCard.OpenMain'),
      Text(this.error).TEST_ID('SmirkyCard.Status')
    );
  }
});
