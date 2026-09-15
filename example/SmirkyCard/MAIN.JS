// MAIN.JS — the built-in cards. Each card is a class: the constructor
// declares its seats with state(initial); compose() returns the card's
// tree using the Loka Node DSL names.
card('first', class {
  constructor() {
    this.script = state('1+1');
    this.result = state('Ready');
  }
  compose() {
    return VStack(
      Text('Card One').TEST_ID('SmirkyCard.Title'),
      Text('This Scene is defined in JavaScript.'),
      EditText(this.script).TEST_ID('SmirkyCard.Script'),
      Button('Run', () => {
        try { this.result.set(String(eval(this.script.get()))); }
        catch (e) { this.result.set('Error: ' + e); }
      }).TEST_ID('SmirkyCard.RunScript'),
      Text(this.result).TEST_ID('SmirkyCard.Result'),
      Button('Run JavaScript', () => go('second')).TEST_ID('SmirkyCard.Run'),
      Button('Reload MAIN.JS', () => reload()).TEST_ID('SmirkyCard.Reload'),
      Text(this.error).TEST_ID('SmirkyCard.Status')
    );
  }
});
card('second', class {
  constructor() {
    this.script = state('2+2');
    this.result = state('Ready');
  }
  compose() {
    return VStack(
      Text('Card Two').TEST_ID('SmirkyCard.Title'),
      Text('This Scene is also defined in JavaScript.'),
      EditText(this.script).TEST_ID('SmirkyCard.Script'),
      Row(
        Button('Run', () => {
          try { this.result.set(String(eval(this.script.get()))); }
          catch (e) { this.result.set('Error: ' + e); }
        }).TEST_ID('SmirkyCard.RunScript'),
        Text(this.result).TEST_ID('SmirkyCard.Result'),
        Button('Run JavaScript', () => go('first')).TEST_ID('SmirkyCard.Run')
      ),
      Text(this.error).TEST_ID('SmirkyCard.Status')
    );
  }
});
