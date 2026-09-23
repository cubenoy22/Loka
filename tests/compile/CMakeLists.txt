# loka_compile_pin(<name> REFUSES <tu> ACCEPTS <tu>)
function(loka_compile_pin NAME)
  cmake_parse_arguments(PIN "" "REFUSES;ACCEPTS" "" ${ARGN})
  if(NOT PIN_REFUSES OR NOT PIN_ACCEPTS OR PIN_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "loka_compile_pin requires REFUSES <tu> ACCEPTS <tu>")
  endif()

  add_library(compilePin_${NAME}_refuses OBJECT EXCLUDE_FROM_ALL ${PIN_REFUSES})
  add_library(compilePin_${NAME}_accepts OBJECT ${PIN_ACCEPTS})
  foreach(kind IN ITEMS refuses accepts)
    set(target compilePin_${NAME}_${kind})
    target_link_libraries(${target} PRIVATE LokaContractTestEnvironment)
    loka_enable_warnings(${target})
    set_target_properties(${target} PROPERTIES
      CXX_STANDARD 98
      CXX_STANDARD_REQUIRED YES
    )
  endforeach()

  if(LOKA_COMPILE_PINS)
    add_test(NAME compileRefuses-${NAME}
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        --config $<CONFIG> --target compilePin_${NAME}_refuses)
    set_tests_properties(compileRefuses-${NAME} PROPERTIES
      WILL_FAIL TRUE RESOURCE_LOCK loka-build-dir)
  endif()
  add_test(NAME compileAccepts-${NAME}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --config $<CONFIG> --target compilePin_${NAME}_accepts)
  set_tests_properties(compileAccepts-${NAME} PROPERTIES
    RESOURCE_LOCK loka-build-dir)
endfunction()

loka_compile_pin(borrowed-state-is-read-only
  REFUSES pins/refuses_set_on_borrowed_state.cpp
  ACCEPTS pins/accepts_get_on_borrowed_state.cpp
)

loka_compile_pin(bindings-need-the-token
  REFUSES pins/refuses_bindings_need_the_token.cpp
  ACCEPTS pins/accepts_bindings_need_the_token.cpp
)

loka_compile_pin(keyed-declarer-is-a-member-of-the-boundary
  REFUSES pins/refuses_keyed_declarer_member.cpp
  ACCEPTS pins/accepts_keyed_declarer_member.cpp
)

loka_compile_pin(keyed-declarer-owner-is-a-boundary
  REFUSES pins/refuses_keyed_declarer_not_a_boundary.cpp
  ACCEPTS pins/accepts_keyed_declarer_member.cpp
)

loka_compile_pin(observablelist-copy
  REFUSES pins/refuses_observablelist_copy.cpp
  ACCEPTS pins/accepts_observablelist_copy.cpp
)

loka_compile_pin(observablelist-assign
  REFUSES pins/refuses_observablelist_assign.cpp
  ACCEPTS pins/accepts_observablelist_assign.cpp
)

loka_compile_pin(mirroredlist-copy
  REFUSES pins/refuses_mirroredlist_copy.cpp
  ACCEPTS pins/accepts_mirroredlist_copy.cpp
)

loka_compile_pin(mirroredlist-assign
  REFUSES pins/refuses_mirroredlist_assign.cpp
  ACCEPTS pins/accepts_mirroredlist_assign.cpp
)

loka_compile_pin(lazyscope-requires-owning-node
  REFUSES pins/refuses_lazyscope_component.cpp
  ACCEPTS pins/accepts_lazyscope_owner.cpp
)

loka_compile_pin(lazyflex-requires-component-props
  REFUSES pins/refuses_lazyflex_component_props.cpp
  ACCEPTS pins/accepts_lazyflex_component_props.cpp
)

loka_compile_pin(tracker-copy
  REFUSES pins/refuses_tracker_copy.cpp
  ACCEPTS pins/accepts_tracker_construction.cpp
)

loka_compile_pin(tracker-assign
  REFUSES pins/refuses_tracker_assign.cpp
  ACCEPTS pins/accepts_tracker_construction.cpp
)

loka_compile_pin(small-object-pool-source-alignment
  REFUSES pins/refuses_small_object_pool_unaligned_source.cpp
  ACCEPTS pins/accepts_small_object_pool_aligned_source.cpp
)

loka_compile_pin(scene-detach-is-seat-owned
  REFUSES pins/refuses_scene_detach.cpp
  ACCEPTS pins/accepts_scene_requests.cpp
)

loka_compile_pin(scene-attach-is-seat-owned
  REFUSES pins/refuses_scene_attach.cpp
  ACCEPTS pins/accepts_scene_requests.cpp
)

loka_compile_pin(scene-unmount-is-seat-owned
  REFUSES pins/refuses_scene_unmount.cpp
  ACCEPTS pins/accepts_scene_requests.cpp
)

loka_compile_pin(scene-lifecycle-is-seat-owned
  REFUSES pins/refuses_scene_lifecycle.cpp
  ACCEPTS pins/accepts_scene_requests.cpp
)

loka_compile_pin(scene-teardown-is-window-owned
  REFUSES pins/refuses_window_scene_teardown.cpp
  ACCEPTS pins/accepts_external_window_scene_teardown.cpp
)

loka_compile_pin(open-file-dialog-props-requires-node-state
  REFUSES pins/refuses_open_file_dialog_props_raw_state.cpp
  ACCEPTS pins/accepts_open_file_dialog_node_state.cpp
)

loka_compile_pin(open-file-dialog-definition-requires-node-state
  REFUSES pins/refuses_open_file_dialog_definition_raw_state.cpp
  ACCEPTS pins/accepts_open_file_dialog_node_state.cpp
)

loka_compile_pin(edittext-requires-write-seat
  REFUSES pins/refuses_edittext_raw_mutable_state.cpp
  ACCEPTS pins/accepts_edittext_write_seat.cpp
)

loka_compile_pin(seat-descriptor-non-node
  REFUSES pins/refuses_seat_non_node.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-private-node
  REFUSES pins/refuses_seat_private_node.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-ambiguous-node
  REFUSES pins/refuses_seat_ambiguous_node.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-incomplete-node
  REFUSES pins/refuses_seat_incomplete_node.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-zero
  REFUSES pins/refuses_seat_zero.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-negative
  REFUSES pins/refuses_seat_negative.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-unterminated
  REFUSES pins/refuses_seat_unterminated.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-missing
  REFUSES pins/refuses_seat_missing.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-missing-direct
  REFUSES pins/refuses_seat_missing_direct.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-descriptor-wrong-wrapper
  REFUSES pins/refuses_seat_wrong_wrapper.cpp
  ACCEPTS pins/accepts_seat_descriptor.cpp
)

loka_compile_pin(seat-ticket-copy
  REFUSES pins/refuses_seat_ticket_copy.cpp
  ACCEPTS pins/accepts_seat_ticket_borrow.cpp
)

loka_compile_pin(derived-node-state-is-read-only
  REFUSES pins/refuses_derived_node_state_set.cpp
  ACCEPTS pins/accepts_derived_node_state_read.cpp
)

loka_compile_pin(derived-node-needs-mutable-input
  REFUSES pins/refuses_derived_node_derived_dependency.cpp
  ACCEPTS pins/accepts_derived_node_mutable_dependency.cpp
)

loka_compile_pin(style-size-vocabulary-is-closed
  REFUSES pins/refuses_style_size_11.cpp
  ACCEPTS pins/accepts_style_size_12.cpp
)

loka_compile_pin(text-style-only-applies-to-text
  REFUSES pins/refuses_canvas_text_style.cpp
  ACCEPTS pins/accepts_text_text_style.cpp
)

loka_compile_pin(style-families-do-not-cross-merge
  REFUSES pins/refuses_cross_family_style_merge.cpp
  ACCEPTS pins/accepts_same_family_style_merge.cpp
)

loka_compile_pin(attributed-block-style
  REFUSES pins/refuses_attributed_block_style.cpp
  ACCEPTS pins/accepts_attributed_block_style.cpp
)

loka_compile_pin(attributed-string-conversion
  REFUSES pins/refuses_attributed_string_conversion.cpp
  ACCEPTS pins/accepts_attributed_string_conversion.cpp
)

loka_compile_pin(reported-set
  REFUSES pins/refuses_reported_set.cpp
  ACCEPTS pins/accepts_reported_set.cpp
)

loka_compile_pin(editor-fact-seat
  REFUSES pins/refuses_editor_fact_seat.cpp
  ACCEPTS pins/accepts_editor_fact_seat.cpp
)

loka_compile_pin(editor-document-construction
  REFUSES pins/refuses_editor_document_construction.cpp
  ACCEPTS pins/accepts_editor_document_construction.cpp
)

loka_compile_pin(editor-document-access
  REFUSES pins/refuses_editor_document_access.cpp
  ACCEPTS pins/accepts_editor_document_access.cpp
)

loka_compile_pin(editor-old-cursor-constructor
  REFUSES pins/refuses_editor_old_cursor_constructor.cpp
  ACCEPTS pins/accepts_editor_old_cursor_constructor.cpp
)

loka_compile_pin(reported-mutable-alias
  REFUSES pins/refuses_reported_mutable_alias.cpp
  ACCEPTS pins/accepts_reported_mutable_alias.cpp
)

loka_compile_pin(reported-props-door
  REFUSES pins/refuses_reported_props_door.cpp
  ACCEPTS pins/accepts_reported_props_door.cpp
)

loka_compile_pin(request-reply-read-only
  REFUSES pins/refuses_request-reply-read-only.cpp
  ACCEPTS pins/accepts_request-reply-read-only.cpp
)

loka_compile_pin(request-no-node-state
  REFUSES pins/refuses_request-no-node-state.cpp
  ACCEPTS pins/accepts_request-no-node-state.cpp
)

loka_compile_pin(editor-requires-request
  REFUSES pins/refuses_editor-requires-request.cpp
  ACCEPTS pins/accepts_editor-requires-request.cpp
)


loka_compile_pin(editor-seam-key
  REFUSES pins/refuses_editor_seam_key.cpp
  ACCEPTS pins/accepts_editor_seam_key.cpp
)

loka_compile_pin(editor-wrong-seam-key
  REFUSES pins/refuses_editor_wrong_seam_key.cpp
  ACCEPTS pins/accepts_editor_wrong_seam_key.cpp
)

loka_compile_pin(editor-context-friend
  REFUSES pins/refuses_editor_context_friend.cpp
  ACCEPTS pins/accepts_editor_context_friend.cpp
)

loka_compile_pin(handler-wrong-seam-key
  REFUSES pins/refuses_handler_wrong_seam_key.cpp
  ACCEPTS pins/accepts_handler_wrong_seam_key.cpp
)

loka_compile_pin(request-wall-direct-command
  REFUSES pins/refuses_request-wall-direct-command.cpp
  ACCEPTS pins/accepts_request-wall-direct-command.cpp
)

loka_compile_pin(request-wall-direct-command-reply
  REFUSES pins/refuses_request-wall-direct-command-reply.cpp
  ACCEPTS pins/accepts_request-wall-direct-command-reply.cpp
)

loka_compile_pin(request-wall-direct-untyped
  REFUSES pins/refuses_request-wall-direct-untyped.cpp
  ACCEPTS pins/accepts_request-wall-direct-untyped.cpp
)

loka_compile_pin(request-wall-direct-untyped-reply
  REFUSES pins/refuses_request-wall-direct-untyped-reply.cpp
  ACCEPTS pins/accepts_request-wall-direct-untyped-reply.cpp
)

loka_compile_pin(request-wall-batch-command
  REFUSES pins/refuses_request-wall-batch-command.cpp
  ACCEPTS pins/accepts_request-wall-batch-command.cpp
)

loka_compile_pin(request-wall-batch-command-reply
  REFUSES pins/refuses_request-wall-batch-command-reply.cpp
  ACCEPTS pins/accepts_request-wall-batch-command-reply.cpp
)

loka_compile_pin(request-wall-batch-untyped
  REFUSES pins/refuses_request-wall-batch-untyped.cpp
  ACCEPTS pins/accepts_request-wall-batch-untyped.cpp
)

loka_compile_pin(request-wall-batch-untyped-reply
  REFUSES pins/refuses_request-wall-batch-untyped-reply.cpp
  ACCEPTS pins/accepts_request-wall-batch-untyped-reply.cpp
)

loka_compile_pin(request-wall-immediate-command
  REFUSES pins/refuses_request-wall-immediate-command.cpp
  ACCEPTS pins/accepts_request-wall-immediate-command.cpp
)

loka_compile_pin(request-wall-immediate-command-reply
  REFUSES pins/refuses_request-wall-immediate-command-reply.cpp
  ACCEPTS pins/accepts_request-wall-immediate-command-reply.cpp
)

loka_compile_pin(request-wall-immediate-untyped
  REFUSES pins/refuses_request-wall-immediate-untyped.cpp
  ACCEPTS pins/accepts_request-wall-immediate-untyped.cpp
)

loka_compile_pin(request-wall-immediate-untyped-reply
  REFUSES pins/refuses_request-wall-immediate-untyped-reply.cpp
  ACCEPTS pins/accepts_request-wall-immediate-untyped-reply.cpp
)

loka_compile_pin(request-binding-sourceless-command
  REFUSES pins/refuses_request-binding-sourceless-command.cpp
  ACCEPTS pins/accepts_request-binding-sourceless-command.cpp
)
